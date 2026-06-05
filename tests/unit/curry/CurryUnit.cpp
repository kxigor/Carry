// =============================================================================
// CurryUnit — value-category & lifetime coverage for curry::policy.
//
// The three policy axes are exercised independently and in combination:
//
// clang-format off
//   storage : by_value | as_passed | by_reference   (how curried args are kept)
//   call    : move | copy                            (how stored args reach fn)
//   target  : by_value | as_passed | by_reference    (how the functor is kept)
// clang-format on
//
// A counting Probe makes every copy / move / destruction observable, so the
// assertions pin down the *exact* value-category behaviour, not just results.
// Reference / dangling correctness is additionally guarded by ASan at runtime
// (dev-debug-asan preset) — none of the tests construct a dangling scenario.
// =============================================================================

#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "curry.hpp"

namespace {

// ----------------------------------------------------------------------------
// Instrumentation
// ----------------------------------------------------------------------------
struct Counters {
  int default_ctor = 0;
  int copy_ctor = 0;
  int move_ctor = 0;
  int copy_assign = 0;
  int move_assign = 0;
  int dtor = 0;
  int alive = 0;
};

Counters g;

void Reset() { g = Counters{}; }

// Special-member counting type. Move marks the source id as -1 so a consumed
// (moved-from) value is observable from the outside.
struct Probe {
  int id = 0;

  Probe() {
    ++g.default_ctor;
    ++g.alive;
  }
  explicit Probe(int value) : id(value) {
    ++g.default_ctor;
    ++g.alive;
  }
  Probe(const Probe& other) : id(other.id) {
    ++g.copy_ctor;
    ++g.alive;
  }
  Probe(Probe&& other) noexcept : id(other.id) {
    other.id = -1;
    ++g.move_ctor;
    ++g.alive;
  }
  Probe& operator=(const Probe& other) {
    id = other.id;
    ++g.copy_assign;
    return *this;
  }
  Probe& operator=(Probe&& other) noexcept {
    id = other.id;
    other.id = -1;
    ++g.move_assign;
    return *this;
  }
  ~Probe() {
    ++g.dtor;
    --g.alive;
  }
};

// ----------------------------------------------------------------------------
// Policy aliases. Coverage note:
//   storage::by_value     -> own_*, functor_ref
//   storage::as_passed    -> pass_*
//   storage::by_reference -> ref_*
//   call::move            -> own_move, pass_move, ref_move
//   call::copy            -> own_copy, pass_copy, ref_copy, functor_ref
//   target::by_value      -> own_*, pass_*
//   target::by_reference  -> ref_*, functor_ref
// ----------------------------------------------------------------------------
namespace cs = curry::storage;
namespace cc = curry::call;
namespace ct = curry::target;

using own_move = curry::policy<cs::by_value, cc::move, ct::by_value>;
using own_copy = curry::policy<cs::by_value, cc::copy, ct::by_value>;
using pass_move = curry::policy<cs::as_passed, cc::move, ct::by_value>;
using pass_copy = curry::policy<cs::as_passed, cc::copy, ct::by_value>;
using ref_move = curry::policy<cs::by_reference, cc::move, ct::by_reference>;
using ref_copy = curry::policy<cs::by_reference, cc::copy, ct::by_reference>;
using functor_ref = curry::policy<cs::by_value, cc::copy, ct::by_reference>;

// Plain test functors (no Probe members -> they do not perturb the counters).
constexpr auto add3 = [](int a, int b, int c) { return a + b + c; };
constexpr auto probe_id = [](Probe p) { return p.id; };

}  // namespace

// =============================================================================
// 1. Functional behaviour (results, partial application, arity)
// =============================================================================

TEST(Functional, PartialThenComplete) {
  auto curried = own_copy::curry(add3, 1, 2);
  EXPECT_EQ(curried(3), 6);
}

TEST(Functional, ReusableUnderCopyPolicy) {
  auto curried = own_copy::curry(add3, 1, 2);
  EXPECT_EQ(curried(3), 6);
  EXPECT_EQ(curried(10), 13);
  EXPECT_EQ(curried(100), 103);
}

TEST(Functional, ZeroCurriedArgs) {
  auto curried = own_copy::curry(add3);
  EXPECT_EQ(curried(1, 2, 3), 6);
}

TEST(Functional, AllArgsCurried) {
  auto curried = own_move::curry(add3, 1, 2, 3);
  EXPECT_EQ(curried(), 6);
}

TEST(Functional, ArgumentOrderIsPreserved) {
  auto sub = [](int a, int b, int c) { return a - b - c; };
  auto curried = own_copy::curry(sub, 10, 3);
  EXPECT_EQ(curried(2), 5);  // 10 - 3 - 2
}

TEST(Functional, Chaining) {
  auto c1 = own_copy::curry(add3, 1);
  auto c2 = own_copy::curry(c1, 2);
  EXPECT_EQ(c2(3), 6);
}

TEST(Functional, StatefulFunctionObject) {
  struct Multiplier {
    int factor;
    int operator()(int a, int b) const { return a * b * factor; }
  };
  auto curried = own_copy::curry(Multiplier{2}, 3);
  EXPECT_EQ(curried(4), 24);
}

// =============================================================================
// 2. storage axis — what happens to curried args at currying time
// =============================================================================

TEST(StorageByValue, LvalueIsCopiedIn) {
  Reset();
  Probe lv{1};
  const int copies = g.copy_ctor;
  const int moves = g.move_ctor;

  auto curried = own_copy::curry(probe_id, lv);

  EXPECT_EQ(g.copy_ctor - copies, 1);  // owned: lvalue copied into storage
  EXPECT_EQ(g.move_ctor - moves, 0);
  EXPECT_EQ(curried(), 1);
}

TEST(StorageByValue, RvalueIsMovedIn) {
  Reset();
  const int copies = g.copy_ctor;

  auto curried = own_move::curry(probe_id, Probe{2});

  EXPECT_EQ(g.copy_ctor - copies, 0);  // never copied
  EXPECT_GE(g.move_ctor, 1);           // moved into storage
  EXPECT_EQ(curried(), 2);
}

TEST(StorageAsPassed, LvalueIsBorrowedNotCopied) {
  Reset();
  Probe lv{7};
  const int copies = g.copy_ctor;
  const int moves = g.move_ctor;

  auto curried = pass_copy::curry(probe_id, lv);

  EXPECT_EQ(g.copy_ctor - copies, 0);  // stored by reference: no copy
  EXPECT_EQ(g.move_ctor - moves, 0);
  EXPECT_EQ(curried(), 7);
}

TEST(StorageAsPassed, RvalueIsOwnedByMove) {
  Reset();
  const int copies = g.copy_ctor;

  auto curried = pass_move::curry(probe_id, Probe{9});

  EXPECT_EQ(g.copy_ctor - copies, 0);
  EXPECT_GE(g.move_ctor, 1);  // rvalue moved into storage
  EXPECT_EQ(curried(), 9);
}

TEST(StorageByReference, NothingIsCopiedOrMoved) {
  Reset();
  Probe lv{3};
  const int copies = g.copy_ctor;
  const int moves = g.move_ctor;

  auto curried = ref_copy::curry(probe_id, lv);

  EXPECT_EQ(g.copy_ctor - copies, 0);
  EXPECT_EQ(g.move_ctor - moves, 0);
  EXPECT_EQ(curried(), 3);
}

// =============================================================================
// 3. call axis — how stored args reach the functor at invocation
// =============================================================================

TEST(CallMove, RvalueArgIsNeverCopied_StoreThenCallBothMove) {
  Reset();
  auto curried = own_move::curry(probe_id, Probe{5});
  const int moves_after_store = g.move_ctor;

  const int result = curried();

  EXPECT_EQ(result, 5);
  EXPECT_EQ(g.copy_ctor, 0);  // honest move path: zero copies
  EXPECT_EQ(g.move_ctor, moves_after_store + 1);  // one extra move into param
}

TEST(CallMove, ConsumesStorage_SingleShot) {
  Reset();
  auto curried = own_move::curry(probe_id, Probe{42});

  EXPECT_EQ(curried(), 42);  // first call moves the stored Probe out
  EXPECT_EQ(curried(), -1);  // storage is now moved-from (observably consumed)
}

TEST(CallCopy, CopiesIntoFunctor_Reusable) {
  Reset();
  Probe lv{4};
  auto curried = own_copy::curry(probe_id, lv);  // +1 copy at store
  const int copies_after_store = g.copy_ctor;

  EXPECT_EQ(curried(), 4);
  EXPECT_EQ(curried(), 4);  // still 4: storage never consumed

  EXPECT_EQ(g.move_ctor, 0);
  EXPECT_EQ(g.copy_ctor, copies_after_store + 2);  // one copy per call
}

// =============================================================================
// 4. Reference semantics — mutation visibility distinguishes the policies
// =============================================================================

TEST(ReferenceSemantics, AsPassedThreadsLvalueByReference) {
  Reset();
  Probe p{1};
  auto bump = [](Probe& q) { q.id += 10; };

  pass_copy::curry(bump, p)();

  EXPECT_EQ(p.id, 11);  // the caller's object was mutated
}

TEST(ReferenceSemantics, ByValueIsolatesTheCaller) {
  Reset();
  Probe p{1};
  auto bump = [](Probe& q) { q.id += 10; };

  own_copy::curry(bump, p)();

  EXPECT_EQ(p.id, 1);  // only the owned copy changed
}

TEST(ReferenceSemantics, ByReferenceThreadsAddress) {
  Reset();
  Probe p{1};
  auto address_of = [](const Probe& q) { return &q; };

  auto curried = ref_copy::curry(address_of, p);

  EXPECT_EQ(curried(), &p);  // same object, zero-copy passthrough
}

TEST(ReferenceSemantics, ReferenceWrapperSurvivesValueStorage) {
  int value = 10;
  auto inc = [](int& acc, int delta) {
    acc += delta;
    return acc;
  };

  auto curried = own_copy::curry(inc, std::ref(value), 5);

  EXPECT_EQ(curried(), 15);
  EXPECT_EQ(value, 15);  // reference_wrapper kept the reference alive
}

// =============================================================================
// 5. target axis — how the functor itself is kept
// =============================================================================

TEST(FunctorStorage, ByValueCopiesTheFunctor) {
  Reset();
  auto fn = [captured = Probe{0}](int x) { return x + captured.id; };
  const int copies = g.copy_ctor;

  auto curried = own_copy::curry(fn, 1);

  EXPECT_EQ(g.copy_ctor - copies, 1);  // functor (and its Probe) copied in
  EXPECT_EQ(curried(), 1);
}

TEST(FunctorStorage, ByReferenceDoesNotCopyTheFunctor) {
  Reset();
  auto fn = [captured = Probe{0}](int x) { return x + captured.id; };
  const int copies = g.copy_ctor;

  auto curried = functor_ref::curry(fn, 1);

  EXPECT_EQ(g.copy_ctor - copies, 0);  // functor borrowed, not copied
  EXPECT_EQ(curried(), 1);
}

TEST(FunctorStorage, RefQualifiedDeliveryFollowsOriginalCategory) {
  struct RefQual {
    int operator()() & { return 1; }
    int operator()() && { return 2; }
  };

  RefQual lvalue_fn;
  EXPECT_EQ(own_move::curry(lvalue_fn)(), 1);  // lvalue functor -> operator()&
  EXPECT_EQ(own_move::curry(RefQual{})(), 2);  // rvalue functor -> operator()&&
  EXPECT_EQ(own_copy::curry(RefQual{})(), 1);  // copy policy always lvalue
}

TEST(FunctorStorage, AsPassedBorrowsLvalueFunctor) {
  Reset();
  auto fn = [captured = Probe{0}](int x) { return x + captured.id; };
  using pol = curry::policy<cs::by_value, cc::copy, ct::as_passed>;
  const int copies = g.copy_ctor;

  auto curried = pol::curry(fn, 1);  // lvalue functor -> borrowed by reference

  EXPECT_EQ(g.copy_ctor - copies, 0);
  EXPECT_EQ(curried(), 1);
}

TEST(FunctorStorage, AsPassedOwnsRvalueFunctorByMove) {
  Reset();
  using pol = curry::policy<cs::by_value, cc::copy, ct::as_passed>;
  const int copies = g.copy_ctor;

  auto curried = pol::curry([captured = Probe{5}]() { return captured.id; });

  EXPECT_EQ(g.copy_ctor - copies, 0);  // rvalue functor never copied
  EXPECT_GE(g.move_ctor, 1);           // moved into storage
  EXPECT_EQ(curried(), 5);
}

// =============================================================================
// 6. other-args (call-time) value categories
// =============================================================================

TEST(OtherArgs, MovePolicyPerfectForwards) {
  Reset();
  auto curried = own_move::curry(probe_id);  // no curried args

  const int before = g.move_ctor;
  EXPECT_EQ(curried(Probe{8}), 8);  // rvalue call-arg is moved into the param
  EXPECT_EQ(g.copy_ctor, 0);
  EXPECT_GE(g.move_ctor - before, 1);
}

TEST(OtherArgs, MovePolicyLeavesLvaluesAsCopies) {
  Reset();
  auto curried = own_move::curry(probe_id);
  Probe lv{8};

  const int copies = g.copy_ctor;
  EXPECT_EQ(curried(lv), 8);  // lvalue call-arg -> copied into by-value param
  EXPECT_EQ(g.copy_ctor - copies, 1);
}

TEST(OtherArgs, FreshArgsArePerfectForwardedUnderCopyPolicy) {
  Reset();
  auto curried = own_copy::curry(probe_id);  // copy policy

  const int copies = g.copy_ctor;
  const int moves = g.move_ctor;
  EXPECT_EQ(curried(Probe{8}), 8);     // fresh rvalue is moved, not copied:
  EXPECT_EQ(g.copy_ctor - copies, 0);  // the call policy governs *stored* args
  EXPECT_GE(g.move_ctor - moves, 1);   // only, never fresh call-time args
}

TEST(OtherArgs, MoveOnlyArgWorksUnderEveryCallPolicy) {
  auto deref = [](std::unique_ptr<int> p) { return *p; };
  // A move-only call-time argument must be accepted regardless of call policy,
  // because fresh args are perfect-forwarded, not routed through the policy.
  EXPECT_EQ(own_move::curry(deref)(std::make_unique<int>(5)), 5);
  EXPECT_EQ(own_copy::curry(deref)(std::make_unique<int>(7)), 7);
}

// =============================================================================
// 7. Value-category zoo for curried args (lvalue / const / prvalue / xvalue)
// =============================================================================

TEST(Categories, ConstLvalueIsAccepted) {
  const int konst = 100;
  auto curried = own_copy::curry(add3, konst, 200);
  EXPECT_EQ(curried(300), 600);
}

TEST(Categories, PrvalueAndXvalueStrings) {
  auto cat = [](std::string a, std::string b) { return a + b; };

  EXPECT_EQ(own_move::curry(cat, std::string("pr"), "value")(), "prvalue");

  std::string x = "x";
  EXPECT_EQ(own_move::curry(cat, std::move(x), "value")(), "xvalue");
}

TEST(Categories, MixedLvalueRvalueWithAsPassed) {
  auto cat = [](const std::string& a, std::string b, const std::string& c) {
    return a + b + c;
  };
  std::string lv = "L";
  auto curried = pass_move::curry(cat, lv, std::string("M"), "R");
  EXPECT_EQ(curried(), "LMR");
}

// =============================================================================
// 8. Lifetime — owning storage leaves nothing alive once it goes out of scope
// =============================================================================

TEST(Lifetime, OwnedProbesAreReleasedWithTheClosure) {
  Reset();
  {
    auto curried = own_copy::curry(probe_id, Probe{1});
    EXPECT_GT(g.alive, 0);  // the stored copy is alive here
    EXPECT_EQ(curried(), 1);
  }
  EXPECT_EQ(g.alive, 0);  // closure destroyed -> storage destroyed, no leak
}

TEST(Lifetime, PerfectForwardingPassthroughArity) {
  auto count = [](auto&&... args) { return sizeof...(args); };
  std::string s = "test";
  auto curried = own_move::curry(count, 1, std::move(s), "literal");
  EXPECT_EQ(curried(4.0, 'c'), std::size_t{5});
}

// =============================================================================
// 9. All 18 invariants — every (storage x call x target) combination must
//    instantiate, run, and produce the correct result. Only lvalue inputs are
//    used so the by_reference policies never dangle; ASan guards the rest.
// =============================================================================

namespace {

template <typename Storage, typename Call, typename Target>
void RunInvariant() {
  using pol = curry::policy<Storage, Call, Target>;
  auto add = [](int a, int b, int c) { return a + b + c; };
  int x = 1;
  int y = 2;
  int z = 3;
  auto curried = pol::curry(add, x, y);  // add, x, y are all lvalues
  EXPECT_EQ(curried(z), 6) << "failed invariant instantiation";
}

}  // namespace

TEST(AllInvariants, EveryCombinationInstantiatesAndRuns) {
  // storage = by_value
  RunInvariant<cs::by_value, cc::move, ct::by_value>();
  RunInvariant<cs::by_value, cc::move, ct::as_passed>();
  RunInvariant<cs::by_value, cc::move, ct::by_reference>();
  RunInvariant<cs::by_value, cc::copy, ct::by_value>();
  RunInvariant<cs::by_value, cc::copy, ct::as_passed>();
  RunInvariant<cs::by_value, cc::copy, ct::by_reference>();
  // storage = as_passed
  RunInvariant<cs::as_passed, cc::move, ct::by_value>();
  RunInvariant<cs::as_passed, cc::move, ct::as_passed>();
  RunInvariant<cs::as_passed, cc::move, ct::by_reference>();
  RunInvariant<cs::as_passed, cc::copy, ct::by_value>();
  RunInvariant<cs::as_passed, cc::copy, ct::as_passed>();
  RunInvariant<cs::as_passed, cc::copy, ct::by_reference>();
  // storage = by_reference
  RunInvariant<cs::by_reference, cc::move, ct::by_value>();
  RunInvariant<cs::by_reference, cc::move, ct::as_passed>();
  RunInvariant<cs::by_reference, cc::move, ct::by_reference>();
  RunInvariant<cs::by_reference, cc::copy, ct::by_value>();
  RunInvariant<cs::by_reference, cc::copy, ct::as_passed>();
  RunInvariant<cs::by_reference, cc::copy, ct::by_reference>();
}

// =============================================================================
// 10. reference_wrapper — std::ref is an explicit "borrow by reference" even
//     through a value-storing policy. The wrapper must be stored as an owned
//     handle and never moved *through* to the referent, under any call policy.
//     (Regression: by_value used std::make_tuple, which unwraps the wrapper to
//     a bare reference that call::move then stole from — see
//     docs/coverage-proof.)
// =============================================================================

namespace {

template <typename Policy>
void CheckRefThreadsAndIsNotStolen() {
  Probe p{10};
  auto read_id = [](const Probe& q) { return q.id; };
  auto curried = Policy::curry(read_id, std::ref(p));

  EXPECT_EQ(curried(), 10);  // the functor sees the referent
  EXPECT_EQ(p.id, 10);       // and it was NOT moved-from
}

}  // namespace

TEST(ReferenceWrapper, NotStolenAcrossStorageAndCall) {
  // std::ref(p) is a prvalue reference_wrapper, so by_reference storage would
  // dangle (it binds a reference to the temporary wrapper); that combination is
  // excluded by precondition. Every owning/borrowing-by-value combination must
  // thread the reference without stealing it.
  CheckRefThreadsAndIsNotStolen<
      curry::policy<cs::by_value, cc::move, ct::by_value>>();
  CheckRefThreadsAndIsNotStolen<
      curry::policy<cs::by_value, cc::copy, ct::by_value>>();
  CheckRefThreadsAndIsNotStolen<
      curry::policy<cs::as_passed, cc::move, ct::by_value>>();
  CheckRefThreadsAndIsNotStolen<
      curry::policy<cs::as_passed, cc::copy, ct::by_value>>();
}

TEST(ReferenceWrapper, ThreadsMutationThroughValueStorage) {
  int value = 0;
  auto inc = [](int& acc) { acc += 1; };
  // by_value + move: the wrapper is moved, the int is mutated through it.
  curry::policy<cs::by_value, cc::move, ct::by_value>::curry(inc,
                                                             std::ref(value))();
  EXPECT_EQ(value, 1);
}
