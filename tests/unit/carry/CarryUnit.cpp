// CarryUnit — storage and value-category coverage for carry::policy.
//
// Probe counters distinguish copying from moving; address and mutation checks
// distinguish owned objects from borrowed ones. Borrowed sources outlive their
// closures, including named objects passed through std::move.

#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "carry.hpp"

namespace {

struct Counters {
  int copy_ctor = 0;
  int move_ctor = 0;
  int alive = 0;
};

Counters g;

void Reset() { g = Counters{}; }

struct Probe {
  int id;

  explicit Probe(int value) : id(value) { ++g.alive; }
  Probe(const Probe &other) : id(other.id) {
    ++g.copy_ctor;
    ++g.alive;
  }
  Probe(Probe &&other) noexcept : id(other.id) {
    other.id = -1;
    ++g.move_ctor;
    ++g.alive;
  }
  ~Probe() { --g.alive; }
};

namespace cs = carry::storage;
namespace cc = carry::call;
namespace ct = carry::target;

using own_move = carry::policy<cs::by_value, cc::move, ct::by_value>;
using own_copy = carry::policy<cs::by_value, cc::copy, ct::by_value>;
using pass_move = carry::policy<cs::as_passed, cc::move, ct::by_value>;

constexpr auto add3 = [](int a, int b, int c) { return a + b + c; };
constexpr auto probe_id = [](Probe p) { return p.id; };

}  // namespace

TEST(Functional, PartialApplicationIsReusableForPureFunction) {
  auto carried = own_copy::carry(add3, 1, 2);
  EXPECT_EQ(carried(3), 6);
  EXPECT_EQ(carried(10), 13);
}

TEST(Functional, ZeroCarriedArgs) {
  auto carried = own_copy::carry(add3);
  EXPECT_EQ(carried(1, 2, 3), 6);
}

TEST(Functional, AllArgsCarried) {
  auto carried = own_move::carry(add3, 1, 2, 3);
  EXPECT_EQ(carried(), 6);
}

TEST(Functional, ArgumentOrderIsPreserved) {
  auto sub = [](int a, int b, int c) { return a - b - c; };
  auto carried = own_copy::carry(sub, 10, 3);
  EXPECT_EQ(carried(2), 5);
}

TEST(Functional, Chaining) {
  auto c1 = own_copy::carry(add3, 1);
  auto c2 = own_copy::carry(c1, 2);
  EXPECT_EQ(c2(3), 6);
}

TEST(Functional, VariadicArityCombinesStoredAndFreshArguments) {
  auto count = [](auto &&...args) { return sizeof...(args); };
  std::string s = "test";
  auto carried = own_move::carry(count, 1, std::move(s), "literal");
  EXPECT_EQ(carried(4.0, 'c'), std::size_t{5});
}

// All 18 policy combinations are observed through object identities, mutations,
// construction counts, and argument/functor value categories.
namespace {

struct ObservingFunctor;

struct Delivery {
  const Probe *argument;
  const ObservingFunctor *target;
  bool argument_is_rvalue;
  bool target_is_rvalue;
};

struct ObservingFunctor {
  Probe state{20};

  template <typename Arg>
  Delivery operator()(Arg &&arg) & {
    ++arg.id;
    ++state.id;
    return {&arg, this, std::is_rvalue_reference_v<Arg &&>, false};
  }

  template <typename Arg>
  Delivery operator()(Arg &&arg) && {
    ++arg.id;
    ++state.id;
    return {&arg, this, std::is_rvalue_reference_v<Arg &&>, true};
  }
};

template <typename Storage, typename Call, typename Target>
struct PolicyCase {
  using policy = carry::policy<Storage, Call, Target>;
  static constexpr bool copies_argument = std::is_same_v<Storage, cs::by_value>;
  static constexpr bool copies_target = std::is_same_v<Target, ct::by_value>;
  static constexpr bool borrows_rvalue_argument =
      std::is_same_v<Storage, cs::by_reference>;
  static constexpr bool borrows_rvalue_target =
      std::is_same_v<Target, ct::by_reference>;
  static constexpr bool moves_on_call = std::is_same_v<Call, cc::move>;
};

using PolicyCases =
    ::testing::Types<PolicyCase<cs::by_value, cc::move, ct::by_value>,
                     PolicyCase<cs::by_value, cc::move, ct::as_passed>,
                     PolicyCase<cs::by_value, cc::move, ct::by_reference>,
                     PolicyCase<cs::by_value, cc::copy, ct::by_value>,
                     PolicyCase<cs::by_value, cc::copy, ct::as_passed>,
                     PolicyCase<cs::by_value, cc::copy, ct::by_reference>,
                     PolicyCase<cs::as_passed, cc::move, ct::by_value>,
                     PolicyCase<cs::as_passed, cc::move, ct::as_passed>,
                     PolicyCase<cs::as_passed, cc::move, ct::by_reference>,
                     PolicyCase<cs::as_passed, cc::copy, ct::by_value>,
                     PolicyCase<cs::as_passed, cc::copy, ct::as_passed>,
                     PolicyCase<cs::as_passed, cc::copy, ct::by_reference>,
                     PolicyCase<cs::by_reference, cc::move, ct::by_value>,
                     PolicyCase<cs::by_reference, cc::move, ct::as_passed>,
                     PolicyCase<cs::by_reference, cc::move, ct::by_reference>,
                     PolicyCase<cs::by_reference, cc::copy, ct::by_value>,
                     PolicyCase<cs::by_reference, cc::copy, ct::as_passed>,
                     PolicyCase<cs::by_reference, cc::copy, ct::by_reference>>;

template <typename Case>
class PolicyMatrix : public ::testing::Test {};

TYPED_TEST_SUITE(PolicyMatrix, PolicyCases);

TYPED_TEST(PolicyMatrix, LvalueOriginsRespectOwnershipAndRemainLvalues) {
  Reset();
  Probe argument{10};
  ObservingFunctor target;
  using policy = typename TypeParam::policy;

  auto carried = policy::carry(target, argument);
  const int expected_copies = static_cast<int>(TypeParam::copies_argument) +
                              static_cast<int>(TypeParam::copies_target);
  EXPECT_EQ(g.copy_ctor, expected_copies);
  EXPECT_EQ(g.move_ctor, 0);

  const Delivery delivered = carried();
  EXPECT_EQ(delivered.argument == &argument, !TypeParam::copies_argument);
  EXPECT_EQ(delivered.target == &target, !TypeParam::copies_target);
  EXPECT_FALSE(delivered.argument_is_rvalue);
  EXPECT_FALSE(delivered.target_is_rvalue);
  EXPECT_EQ(delivered.argument->id, 11);
  EXPECT_EQ(delivered.target->state.id, 21);
  EXPECT_EQ(argument.id, TypeParam::copies_argument ? 10 : 11);
  EXPECT_EQ(target.state.id, TypeParam::copies_target ? 20 : 21);
  EXPECT_EQ(g.copy_ctor, expected_copies);
  EXPECT_EQ(g.move_ctor, 0);
}

TYPED_TEST(PolicyMatrix, RvalueOriginsRespectOwnershipAndCallPolicy) {
  Reset();
  Probe argument{10};
  ObservingFunctor target;
  using policy = typename TypeParam::policy;

  // std::move changes the category, not the lifetime. Named sources remain
  // alive until after carried is destroyed, including for by_reference.
  auto carried = policy::carry(std::move(target), std::move(argument));
  const int expected_moves =
      static_cast<int>(!TypeParam::borrows_rvalue_argument) +
      static_cast<int>(!TypeParam::borrows_rvalue_target);
  EXPECT_EQ(g.copy_ctor, 0);
  EXPECT_EQ(g.move_ctor, expected_moves);

  const Delivery delivered = carried();
  EXPECT_EQ(delivered.argument == &argument,
            TypeParam::borrows_rvalue_argument);
  EXPECT_EQ(delivered.target == &target, TypeParam::borrows_rvalue_target);
  EXPECT_EQ(delivered.argument_is_rvalue, TypeParam::moves_on_call);
  EXPECT_EQ(delivered.target_is_rvalue, TypeParam::moves_on_call);
  EXPECT_EQ(delivered.argument->id, 11);
  EXPECT_EQ(delivered.target->state.id, 21);
  EXPECT_EQ(argument.id, TypeParam::borrows_rvalue_argument ? 11 : -1);
  EXPECT_EQ(target.state.id, TypeParam::borrows_rvalue_target ? 21 : -1);
  // Receiving an rvalue reference does not itself move-construct anything.
  EXPECT_EQ(g.copy_ctor, 0);
  EXPECT_EQ(g.move_ctor, expected_moves);
}

}  // namespace

TEST(CallMove, ValueParameterConsumesOwnedRvalueOnEachCall) {
  Reset();
  auto carried = own_move::carry(probe_id, Probe{42});
  EXPECT_EQ(g.copy_ctor, 0);
  EXPECT_EQ(g.move_ctor, 1);

  EXPECT_EQ(carried(), 42);
  EXPECT_EQ(g.move_ctor, 2);
  EXPECT_EQ(carried(), -1);  // the first invocation consumed the stored Probe
  EXPECT_EQ(g.move_ctor, 3);
  EXPECT_EQ(g.copy_ctor, 0);
}

TEST(CallCopy, ValueParameterCopiesOwnedRvalueOnEachCall) {
  Reset();
  auto carried = own_copy::carry(probe_id, Probe{4});
  EXPECT_EQ(g.copy_ctor, 0);
  EXPECT_EQ(g.move_ctor, 1);

  EXPECT_EQ(carried(), 4);
  EXPECT_EQ(g.copy_ctor, 1);
  EXPECT_EQ(carried(), 4);
  EXPECT_EQ(g.copy_ctor, 2);
  EXPECT_EQ(g.move_ctor, 1);
}

namespace {

template <typename Policy>
class OwningCallPolicies : public ::testing::Test {};

using OwningPolicies = ::testing::Types<own_move, own_copy>;
TYPED_TEST_SUITE(OwningCallPolicies, OwningPolicies);

enum class Category { lvalue, const_lvalue, rvalue, const_rvalue };

template <typename T>
constexpr Category CategoryOf() {
  if constexpr (std::is_lvalue_reference_v<T>) {
    return std::is_const_v<std::remove_reference_t<T>> ? Category::const_lvalue
                                                       : Category::lvalue;
  } else {
    return std::is_const_v<std::remove_reference_t<T>> ? Category::const_rvalue
                                                       : Category::rvalue;
  }
}

TYPED_TEST(OwningCallPolicies, FreshArgumentsPreserveCvAndValueCategories) {
  auto carried = TypeParam::carry(
      [](auto &&value) { return CategoryOf<decltype(value)>(); });
  int value = 1;
  const int constant = 2;

  EXPECT_EQ(carried(value), Category::lvalue);
  EXPECT_EQ(carried(constant), Category::const_lvalue);
  EXPECT_EQ(carried(std::move(value)), Category::rvalue);
  EXPECT_EQ(carried(std::move(constant)), Category::const_rvalue);
  EXPECT_EQ(carried(3), Category::rvalue);
}

TYPED_TEST(OwningCallPolicies,
           FreshValueParameterCopiesLvaluesAndMovesRvalues) {
  Reset();
  auto carried = TypeParam::carry(probe_id);
  Probe value{8};

  EXPECT_EQ(carried(value), 8);
  EXPECT_EQ(value.id, 8);
  EXPECT_EQ(g.copy_ctor, 1);
  EXPECT_EQ(g.move_ctor, 0);

  EXPECT_EQ(carried(std::move(value)), 8);
  EXPECT_EQ(value.id, -1);
  EXPECT_EQ(g.copy_ctor, 1);
  EXPECT_EQ(g.move_ctor, 1);
}

TYPED_TEST(OwningCallPolicies, FreshMoveOnlyValueParameterIsAccepted) {
  auto consume = [](std::unique_ptr<int> p) { return *p; };
  EXPECT_EQ(TypeParam::carry(consume)(std::make_unique<int>(5)), 5);
}

TYPED_TEST(OwningCallPolicies, MoveOnlyFunctorCanBeOwned) {
  auto carried = TypeParam::carry(
      [p = std::make_unique<int>(7)](int x) { return *p + x; }, 2);
  static_assert(!std::is_copy_constructible_v<decltype(carried)>);
  EXPECT_EQ(carried(), 9);

  auto moved = std::move(carried);
  EXPECT_EQ(moved(), 9);
}

TYPED_TEST(OwningCallPolicies,
           StoredMoveOnlyArgumentCanBeReadWithoutConsumption) {
  auto read = [](const std::unique_ptr<int> &p) { return *p; };
  auto carried = TypeParam::carry(read, std::make_unique<int>(11));
  static_assert(!std::is_copy_constructible_v<decltype(carried)>);
  EXPECT_EQ(carried(), 11);
  EXPECT_EQ(carried(), 11);
}

}  // namespace

TEST(CallMove, StoredMoveOnlyArgumentCanBeConsumed) {
  auto consume = [](std::unique_ptr<int> p) { return p ? *p : -1; };
  auto carried = own_move::carry(consume, std::make_unique<int>(9));
  EXPECT_EQ(carried(), 9);
  EXPECT_EQ(carried(), -1);
}

TEST(Categories, StorageControlsConstnessOfLvalueOrigins) {
  const int value = 10;
  auto category = [](auto &&arg) { return CategoryOf<decltype(arg)>(); };
  // by_value owns a decayed copy; borrowing retains the source's constness.
  EXPECT_EQ(own_move::carry(category, value)(), Category::lvalue);
  EXPECT_EQ(own_copy::carry(category, value)(), Category::lvalue);
  EXPECT_EQ((carry::policy<cs::as_passed, cc::move, ct::by_value>::carry(
                category, value)()),
            Category::const_lvalue);
  EXPECT_EQ((carry::policy<cs::as_passed, cc::copy, ct::by_value>::carry(
                category, value)()),
            Category::const_lvalue);
  EXPECT_EQ((carry::policy<cs::by_reference, cc::move, ct::by_value>::carry(
                category, value)()),
            Category::const_lvalue);
  EXPECT_EQ((carry::policy<cs::by_reference, cc::copy, ct::by_value>::carry(
                category, value)()),
            Category::const_lvalue);
}

TEST(Categories, PrvalueAndXvalueStrings) {
  auto cat = [](std::string a, std::string b) { return a + b; };
  EXPECT_EQ(own_move::carry(cat, std::string("pr"), "value")(), "prvalue");

  std::string x = "x";
  EXPECT_EQ(own_move::carry(cat, std::move(x), "value")(), "xvalue");
}

TEST(Categories, MixedLvalueRvalueWithAsPassed) {
  auto cat = [](auto &&a, auto &&b, auto &&c, auto &&suffix) {
    EXPECT_EQ(CategoryOf<decltype(a)>(), Category::lvalue);
    EXPECT_EQ(CategoryOf<decltype(b)>(), Category::rvalue);
    EXPECT_EQ(CategoryOf<decltype(c)>(), Category::const_lvalue);
    EXPECT_EQ(CategoryOf<decltype(suffix)>(), Category::rvalue);
    return a + b + c + suffix;
  };
  std::string lv = "L";
  auto carried = pass_move::carry(cat, lv, std::string("M"), "R");
  EXPECT_EQ(carried(std::string("!")), "LMR!");
}

TEST(Lifetime, OwnedProbesAreReleasedWithTheClosure) {
  Reset();
  {
    auto carried = own_copy::carry(probe_id, Probe{1});
    EXPECT_EQ(g.alive, 1);
    EXPECT_EQ(carried(), 1);
    EXPECT_EQ(g.alive, 1);
  }
  EXPECT_EQ(g.alive, 0);
}

// reference_wrapper borrows its referent; copying the wrapper never extends the
// referent's lifetime. Keep both the referent and named wrapper alive here.
namespace {

template <typename Policy>
void CheckRefThreadsAndIsNotStolen() {
  Reset();
  Probe p{10};
  auto wrapper = std::ref(p);
  // A value parameter makes an accidental move through the wrapper observable.
  auto carried = Policy::carry(probe_id, std::move(wrapper));
  EXPECT_EQ(g.copy_ctor, 0);
  EXPECT_EQ(g.move_ctor, 0);

  EXPECT_EQ(carried(), 10);
  EXPECT_EQ(p.id, 10);
  EXPECT_EQ(g.copy_ctor, 1);
  EXPECT_EQ(g.move_ctor, 0);

  EXPECT_EQ(carried(), 10);
  EXPECT_EQ(p.id, 10);
  EXPECT_EQ(g.copy_ctor, 2);
  EXPECT_EQ(g.move_ctor, 0);
}

using WrapperPolicies =
    ::testing::Types<carry::policy<cs::by_value, cc::move, ct::by_value>,
                     carry::policy<cs::by_value, cc::copy, ct::by_value>,
                     carry::policy<cs::as_passed, cc::move, ct::by_value>,
                     carry::policy<cs::as_passed, cc::copy, ct::by_value>,
                     carry::policy<cs::by_reference, cc::move, ct::by_value>,
                     carry::policy<cs::by_reference, cc::copy, ct::by_value>>;

template <typename Policy>
class ReferenceWrapper : public ::testing::Test {};

TYPED_TEST_SUITE(ReferenceWrapper, WrapperPolicies);

TYPED_TEST(ReferenceWrapper, NeverMovesFromItsReferent) {
  CheckRefThreadsAndIsNotStolen<TypeParam>();
}

TYPED_TEST(OwningCallPolicies,
           ReferenceWrapperThreadsMutationThroughOwnedStorage) {
  int value = 10;
  auto inc = [](int &acc, int delta) { acc += delta; };
  auto carried = TypeParam::carry(inc, std::ref(value), 5);
  carried();
  EXPECT_EQ(value, 15);
  carried();
  EXPECT_EQ(value, 20);
}

TYPED_TEST(OwningCallPolicies, ConstReferenceWrapperPreservesReferentIdentity) {
  const int value = 42;
  auto address = [](const int &arg) { return &arg; };
  auto carried = TypeParam::carry(address, std::cref(value));
  EXPECT_EQ(carried(), &value);
}

}  // namespace
