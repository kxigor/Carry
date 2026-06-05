# Exhaustive Coverage of Value-Category Cases — a formal argument

This document argues, with reference to the C++ standard (N4950, C++23), that the
unit suite in [`tests/unit/curry/CurryUnit.cpp`](../tests/unit/curry/CurryUnit.cpp)
exercises the **complete space** of value-category behaviours of

```cpp
curry::policy<Storage, Call, Target>::curry(functor, curried_args...)(other_args...)
```

over its **valid** input domain, and **every combination** thereof — not merely a
sample. Clause references in `[brackets]` are N4950 stable tags.

---

## 1. Object under test

Let the *system under test* be the function

```
Φ(S, C, T; f, a₁…aₙ; b₁…bₘ)
```

where `S, C, T` are the three policies, `f` the functor, `aᵢ` the curried
arguments, and `bⱼ` the call-time arguments. `Φ` produces an *observable*
consisting of, for every object involved: the constructor selected
(copy/move/none), whether it is borrowed (a reference) or owned (a value), its
lifetime, and the value category at which it reaches `f`.

The library source is small and total (no runtime branching except one
`if constexpr`, [stmt.if]/p2, which is resolved at instantiation, not at run
time). Therefore `Φ`’s behaviour is a *pure function of types and value
categories*, fixed at compile time. This is what makes exhaustive reasoning
possible.

---

## 2. The input space

### 2.1 Policy axes (finite, enumerable)

```
S ∈ { by_value, as_passed, by_reference }      |S| = 3
C ∈ { move, copy }                              |C| = 2
T ∈ { by_value, as_passed, by_reference }      |T| = 3
```

The policy triple is a **global** selector: the same `S,C,T` apply to every
slot. Hence it cannot be factored away and all `|S|·|C|·|T| = 18` triples must be
realized. (Test `AllInvariants.EveryCombinationInstantiatesAndRuns`.)

### 2.2 Value categories collapse to two classes

By [basic.lval]/p1 every expression is exactly one of **lvalue**, **xvalue**,
**prvalue**; *rvalue* := xvalue ∪ prvalue. The library only ever observes an
argument through a forwarding reference `X&&` ([temp.deduct.call]/p3,
[dcl.ref]/p6 reference collapsing):

> If `P` is a forwarding reference and the argument is an lvalue, the type
> “lvalue reference to `A`” is used for deduction.

Thus an lvalue of type `A` deduces `X = A&`, while **both** an xvalue and a
prvalue deduce `X = A`. Define the equivalence

```
e₁ ∼ e₂  ⟺  e₁ and e₂ deduce the same X against X&&.
```

The quotient has exactly **two** classes, which we name

```
L = { lvalues }              (deduces A&,  is_lvalue_reference_v<X> = true)
R = { xvalues, prvalues }    (deduces A,   is_lvalue_reference_v<X> = false)
```

Everything `Φ` does downstream is a function of `X` alone (it forwards, stores,
and re-forwards using `X`). Therefore **xvalue and prvalue are
behaviourally identical to the library**, and one representative of each class
suffices per slot. (Test `Categories.PrvalueAndXvalueStrings` exhibits one
xvalue and one prvalue and confirms identical results — empirical corroboration
of this reduction.)

`const`-qualification is *not* an independent library branch: the library never
inspects `const`; copy-vs-move selection is delegated to the argument type’s own
special-member overload resolution ([over.match.best]). `const` is therefore a
property of the *type parameter*, covered by type choice
(`Categories.ConstLvalueIsAccepted`), not a new case of `Φ`.

### 2.3 Type-shape cases that the *transform itself* distinguishes

The storage transform is not type-uniform: `std::make_tuple` applies
`unwrap_ref_decay_t` ([tuple.creation]/p2, [refwrap.general]), i.e. it
**decays** ([conv.array], [conv.func], [meta.trans.other]) and **unwraps**
`reference_wrapper`. So within class `R`/`L` the type shapes

```
{ ordinary object type, array type, reference_wrapper<T> }
```

select different stored element types and must each appear at least once under
value storage. (Tests `Lifetime.PerfectForwardingPassthroughArity` — a
`const char[8]` literal decays to `const char*`; and
`ReferenceSemantics.ReferenceWrapperSurvivesValueStorage` — `reference_wrapper`
is unwrapped to `T&`.) These are precisely the shapes for which a naive
`std::forward<Original>` on the decayed object would be ill-formed (the reason
`call::move::forward` keys off the value *category*, not the original type).

---

## 3. Factorization Lemma (why a finite suite covers unbounded arity)

**Lemma.** Fix `(S, C, T)`. The observable `Φ` is the Cartesian product of
*independent per-slot* transforms:

```
Φ = τ_f(T, C, cat_f)  ×  ∏ᵢ τ_a(S, C, cat_{aᵢ})  ×  ∏ⱼ τ_b(C, cat_{bⱼ})
```

*Proof.* `store` builds the tuple by pack expansion ([temp.variadic]); element
`i` is initialized solely from argument `i` ([tuple.cnstr]), with no
cross-element dependence. Invocation uses `std::apply`, which expands
`std::get<I>` over the index pack ([tuple.apply]) and applies
`Call::forward<Xᵢ>` to each element independently. The functor slot is a separate
one-element tuple. No step reads more than one slot at a time. Hence each slot’s
contribution to the observable depends only on its own `(policy, category[,
shape])`, independent of the other slots’ count, order, or categories. ∎

**Corollary (covering-array sufficiency).** To cover *all* argument
combinations it suffices to cover, for each fixed policy triple, the **domain of
each slot-factor** at least once. The full Cartesian product over arguments
need **not** be enumerated; in particular arbitrary arity and arbitrary
lvalue/rvalue mixes are covered once each factor’s domain is.
(Order-preservation, which the lemma does not assert, is checked separately by
`Functional.ArgumentOrderIsPreserved`; mixed categories in one call by
`Categories.MixedLvalueRvalueWithAsPassed`; arbitrary arity passthrough by
`Lifetime.PerfectForwardingPassthroughArity`.)

The slot-factor domains are therefore:

| Factor | Domain | Size |
|--------|--------|------|
| curried-arg `τ_a` | `S × C × {L,R}` | 3·2·2 = 12 |
| functor `τ_f`     | `T × C × {L,R}` | 3·2·2 = 12 |
| other-arg `τ_b`   | `C × {L,R}`     | 2·2 = 4 |
| global triple     | `S × C × T`     | 18 |

---

## 4. The valid sub-domain (precondition)

`by_reference` storage/target binds a reference. With an `R` origin that
reference binds to a **temporary**, whose lifetime ends at the end of the
full-expression containing the `curry(...)` call ([class.temporary]/p4); no
lifetime-extension exception applies across the function return
([class.temporary]/p6). Any later use is access to an object outside its
lifetime — **undefined behaviour** ([basic.life]/p6). Hence

```
(S = by_reference, cat = R)   and   (T = by_reference, cat = R)
```

are **excluded from the valid input space by precondition** (documented in the
header and README). `as_passed` with `R` is *not* excluded — it owns by move
([tuple.creation], move-construction of the value element), and `by_reference`
with `L` is valid provided the lvalue outlives the closure, which every relevant
test guarantees by lexical scoping. The theorem below concerns the valid space;
the excluded two cells are additionally guarded at run time by AddressSanitizer.

After exclusion the factor domains have `12−2 = 10` (curried), `10` (functor),
`4` (other) live cells.

---

## 5. Per-factor outcomes derived from the standard, with the covering test

### 5.1 Curried-argument factor `τ_a(S, C, cat)`

**Store-time** (`S × cat`):

| S | cat | element type / effect (standard) | test |
|---|-----|----------------------------------|------|
| by_value | L | `unwrap_ref_decay_t`, **copy**-constructed [tuple.creation] | `StorageByValue.LvalueIsCopiedIn` |
| by_value | R | `unwrap_ref_decay_t`, **move**-constructed | `StorageByValue.RvalueIsMovedIn` |
| as_passed | L | element type `A&` — **reference**, no copy/move | `StorageAsPassed.LvalueIsBorrowedNotCopied` |
| as_passed | R | element type `A` — **move**-constructed | `StorageAsPassed.RvalueIsOwnedByMove` |
| by_reference | L | `forward_as_tuple` ⇒ `A&` — **reference** | `StorageByReference.NothingIsCopiedOrMoved` |
| by_reference | R | — | *excluded (§4)* |

**Call-time delivery** (`C × cat`), `Call::forward<X>` ([forward],
[meta.unary.comp]):

| C | cat | delivery | test |
|---|-----|----------|------|
| move | R | `std::move(stored)` → rvalue (single-shot) | `CallMove.RvalueArgIsNeverCopied`, `CallMove.ConsumesStorage_SingleShot` |
| move | L | returns `stored` → lvalue | ≡ `(copy,L)`, see note | 
| copy | L | returns `stored` → lvalue | `CallCopy.CopiesIntoFunctor_Reusable` |
| copy | R | returns `stored` → lvalue (downgrade) | `OtherArgs.CopyPolicyDowngradesFreshRvaluesToCopies` |

*Note `(move,L) ≡ (copy,L)`*: in the header, `call::move::forward`’s
`is_lvalue_reference` branch is the statement `return arg;`, **token-identical**
to `call::copy::forward`. They are the same function on class `L`; one test of
lvalue delivery therefore covers both. Each combination is additionally
*instantiated and run* by `AllInvariants`.

### 5.2 Functor factor `τ_f(T, C, cat)`

| T | cat | effect | test |
|---|-----|--------|------|
| by_value | L | functor **copied** in | `FunctorStorage.ByValueCopiesTheFunctor` |
| by_value | R | functor **moved** in | ≡ `(as_passed,R)`; exercised by `RefQualifiedDelivery` |
| as_passed | L | functor **borrowed** | `FunctorStorage.AsPassedBorrowsLvalueFunctor` |
| as_passed | R | functor **moved** in | `FunctorStorage.AsPassedOwnsRvalueFunctorByMove` |
| by_reference | L | functor **borrowed** | `FunctorStorage.ByReferenceDoesNotCopyTheFunctor` |
| by_reference | R | — | *excluded (§4)* |

Functor **delivery** category (`C × cat`) is observed through ref-qualified
`operator()` ([over.match.funcs], [class.mfct.non.static]): `move,L → operator()&`,
`move,R → operator()&&`, `copy,* → operator()&` — all three in
`FunctorStorage.RefQualifiedDeliveryFollowsOriginalCategory`.

`(by_value,R) ≡ (as_passed,R)`: for an `R` origin of an ordinary (non-array,
non-`reference_wrapper`) type, `unwrap_ref_decay_t<A> = A`, so `by_value` and
`as_passed` both yield an owned, move-constructed element — the same transform.

### 5.3 Other-argument factor `τ_b(C, cat)`

| C | cat | delivery | test |
|---|-----|----------|------|
| move | R | perfect-forwarded as rvalue | `OtherArgs.MovePolicyPerfectForwards` |
| move | L | forwarded as lvalue (copy) | `OtherArgs.MovePolicyLeavesLvaluesAsCopies` |
| copy | R | downgraded to lvalue (copy) | `OtherArgs.CopyPolicyDowngradesFreshRvaluesToCopies` |
| copy | L | lvalue | exercised by `AllInvariants` (`curried(z)`, `z` an lvalue) |

### 5.4 Semantic correctness of borrow vs. own (identity of the referent)

Independently of counts, the *identity* contract is checked:

- borrow threads the **same object** — `ReferenceSemantics.AsPassedThreadsLvalueByReference`
  (mutation visible), `ReferenceSemantics.ByReferenceThreadsAddress`
  (`&param == &caller`);
- own **isolates** the caller — `ReferenceSemantics.ByValueIsolatesTheCaller`.

### 5.5 Lifetime

`Lifetime.OwnedProbesAreReleasedWithTheClosure` asserts that owned subobjects are
destroyed exactly when the closure is ([expr.prim.lambda.capture],
[class.dtor]) — `alive == 0` after scope exit, i.e. no leak and no early death.
The whole suite runs under ASan + UBSan, which would flag any out-of-lifetime
access in the borrow cases.

---

## 6. Theorem

> **Theorem.** The suite covers every behaviour of `Φ` over the valid input
> space, and every combination of slot behaviours.

*Proof.* By §3 (Lemma), for each fixed policy triple `Φ` factors into independent
per-slot transforms; covering each factor’s live domain once covers all
argument combinations (Corollary). §5 exhibits, for each cell of each factor
domain (`τ_a`: 10 live cells, `τ_f`: 10, `τ_b`: 4), a test asserting the
standard-mandated outcome — using, where two cells are provably the same
function (§5.1, §5.2 notes), the shared witness. §2.2 reduces value categories to
the two classes `{L,R}` actually distinguished by [temp.deduct.call], with
xvalue/prvalue equivalence corroborated empirically; §2.3 adds the three storage
type-shapes, each witnessed. The 18 global triples are each instantiated and run
(`AllInvariants`). The excluded cells (§4) are exactly the standard’s
undefined-behaviour region and lie outside the valid domain. Therefore the union
of tested cases equals the valid input space and, by the Lemma, all their
combinations. ∎

---

## 7. Empirical corroboration

The structural argument above establishes *sufficiency*; coverage tooling
confirms *no path is left unexecuted* (necessary condition):

```
lcov on the ci-coverage build, filtered to include/curry.hpp:
  lines ....... 100.0%  (23 / 23)
  functions ... 100.0%  (257 / 257 template instantiations)
  branches .... no runtime branches  (the sole branch is `if constexpr`)
```

Run reproduced via:

```bash
cmake --preset ci-coverage && cmake --build build/ci-coverage
ctest --test-dir build/ci-coverage
lcov --capture --directory build/ci-coverage -o cov.info
lcov --extract cov.info '*/include/curry.hpp' -o curry.info && lcov --list curry.info
```

Line coverage alone is necessary but not sufficient (it cannot see the missing
*type/category* cases of a template); the §6 enumeration supplies the
sufficiency that line coverage cannot.
