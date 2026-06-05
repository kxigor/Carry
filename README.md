# Curry

A header-only C++23 library for **function currying / partial application** with
explicit, composable control over **value categories** and **lifetimes**.

Most currying helpers make one hard-wired choice about whether bound arguments
are copied, moved, or referenced. Curry makes that choice a *policy* you select
per call site, so ownership is honest and visible in the type.

```cpp
#include "curry.hpp"

auto add = [](int a, int b, int c) { return a + b + c; };

using fast = curry::policy<curry::storage::by_value,
                           curry::call::move,
                           curry::target::by_value>;

auto add12 = fast::curry(add, 1, 2);
int  six   = add12(3);            // 1 + 2 + 3
```

## The policy model

Currying behaviour is the product of three **orthogonal axes**, bundled into one
reusable type `curry::policy<Storage, Call, Target>`:

| Axis        | Namespace          | Options                                  | Governs                                   |
|-------------|--------------------|------------------------------------------|-------------------------------------------|
| **storage** | `curry::storage`   | `by_value`, `as_passed`, `by_reference`  | how curried arguments are kept            |
| **call**    | `curry::call`      | `move`, `copy`                           | how stored arguments reach the functor    |
| **target**  | `curry::target`    | `by_value`, `as_passed`, `by_reference`  | how the functor itself is kept            |

That is `3 × 2 × 3 = 18` distinct, well-defined configurations.

### storage / target — own vs. borrow

| Policy         | lvalue argument            | rvalue argument        |
|----------------|----------------------------|------------------------|
| `by_value`     | copied (owned)             | moved (owned)          |
| `as_passed`    | referenced (borrowed)      | moved (owned)          |
| `by_reference` | referenced (borrowed)      | referenced (borrowed)  |

`target` uses the same mechanism for the functor (it is held as a one-element
tuple).

### call — deliver by move vs. by copy

| Policy | rvalue-origin argument | lvalue-origin argument | Reusable?                  |
|--------|------------------------|------------------------|----------------------------|
| `move` | moved into the functor | passed as lvalue       | single-shot for moved args |
| `copy` | passed as lvalue       | passed as lvalue       | yes                        |

The call policy governs **stored (curried) arguments only**. Fresh call-time
arguments are always perfect-forwarded, so a move-only argument works under any
policy.

## Usage

`curry::policy<...>` is a value-less bundle; bind it to a name and reuse it:

```cpp
namespace cs = curry::storage;
namespace cc = curry::call;
namespace ct = curry::target;

using owning  = curry::policy<cs::by_value,     cc::copy, ct::by_value>;
using piping  = curry::policy<cs::as_passed,    cc::move, ct::by_value>;
using viewing = curry::policy<cs::by_reference, cc::copy, ct::by_reference>;

auto f = owning::curry(fn, a, b);   // owns a, b; reusable
f(c);
f(d);                               // fine — copy policy keeps storage intact
```

- **Partial application** of any arity, including all-or-none:
  ```cpp
  owning::curry(add, 1, 2, 3)();    // all bound
  owning::curry(add)(1, 2, 3);      // none bound
  ```
- **Chaining**: a curried callable is itself a functor.
  ```cpp
  auto g = owning::curry(add, 1);
  auto h = owning::curry(g, 2);
  h(3);                             // 6
  ```
- **References** survive value storage via `std::reference_wrapper`:
  ```cpp
  int acc = 0;
  owning::curry(accumulate, std::ref(acc), 5)();   // mutates acc
  ```

## Choosing a policy

- **Default / safe**: `by_value, copy, by_value` — owns everything, reusable,
  never dangles.
- **Pipelines / sinks**: `as_passed, move, by_value` — the "honest" ownership:
  temporaries are owned and moved through, named objects are borrowed.
- **Continuations / immediate use**: `by_reference, …` — zero-copy, but the
  callable must not outlive what it borrows.

## Lifetime caveats

These follow directly from the standard and are **preconditions**, not bugs:

- **`by_reference` + an rvalue** stores a reference to a temporary whose lifetime
  ends at the end of the currying full-expression ([class.temporary]); using the
  callable afterwards is a dangling reference. Use `by_reference` only with
  lvalues that outlive the callable.
- **`as_passed` + an rvalue is safe** — it owns the temporary by move.
  `as_passed` + an *lvalue* borrows it, so that lvalue must outlive the callable.
- **`std::ref(x)`** is the explicit way to borrow through a value-storing policy:
  the reference is threaded safely (carried inside the `reference_wrapper`) and
  is never moved through, even under `call::move`.

The test suite runs under AddressSanitizer + UBSan to catch any accidental
violation.

## Requirements

- A C++23 compiler (developed against GCC 16). Uses class-type init-capture with
  an explicit template parameter list on the lambda, `std::apply`, and concepts.

## Build & test

The project uses CMake presets (Ninja):

```bash
# Debug + tests + Address/UB sanitizers
cmake --preset dev-debug-asan
cmake --build build/dev-debug-asan
ctest --test-dir build/dev-debug-asan          # FormatCheck, TidyCheck, unit tests

# Coverage (format/tidy disabled by design)
cmake --preset ci-coverage
cmake --build build/ci-coverage
ctest --test-dir build/ci-coverage
```

As a header-only dependency, link the interface target:

```cmake
add_subdirectory(Curry)
target_link_libraries(your_target PRIVATE curry::curry)
```

## Layout

```
include/curry.hpp            the library (Doxygen-documented public interface)
tests/unit/curry/            value-category & lifetime unit tests
docs/coverage-proof.md       formal argument that the suite covers every case
cmake/, CMakePresets.json    build system
```

See [docs/coverage-proof.md](docs/coverage-proof.md) for a standard-referenced
proof that the unit tests exercise the complete space of value-category cases
and their combinations.
