# Carry

A header-only C++23 library for **function currying / partial application** with
explicit, composable control over **value categories** and **lifetimes**.

Most currying helpers make one hard-wired choice about whether bound arguments
are copied, moved, or referenced. Carry makes that choice a *policy* you select
per call site. The policy controls how objects are stored and passed; the objects
themselves may still contain borrowed references.

```cpp
#include "carry.hpp"

auto add = [](int a, int b, int c) { return a + b + c; };

using fast = carry::policy<carry::storage::by_value,
                           carry::call::move,
                           carry::target::by_value>;

auto add12 = fast::carry(add, 1, 2);
int  six   = add12(3);            // 1 + 2 + 3
```

## The policy model

Currying behaviour is the product of three **orthogonal axes**, bundled into one
reusable type `carry::policy<Storage, Call, Target>`:

| Axis        | Namespace          | Options                                  | Governs                                   |
|-------------|--------------------|------------------------------------------|-------------------------------------------|
| **storage** | `carry::storage`   | `by_value`, `as_passed`, `by_reference`  | how curried arguments are kept            |
| **call**    | `carry::call`      | `move`, `copy`                           | how stored arguments and the functor are used |
| **target**  | `carry::target`    | `by_value`, `as_passed`, `by_reference`  | how the functor itself is kept            |

That is `3 × 2 × 3 = 18` distinct, well-defined configurations.

### storage / target — own vs. borrow

| Policy         | lvalue argument       | rvalue argument               |
|----------------|-----------------------|-------------------------------|
| `by_value`     | copied (owned value)  | owned value, moved if possible |
| `as_passed`    | referenced (borrowed) | owned value, moved if possible |
| `by_reference` | referenced (borrowed) | referenced (borrowed)         |

`target` uses the same mechanism for the functor (it is held as a one-element
tuple). Owning a stored value does not imply owning everything it refers to;
`by_value` also decays arrays and functions to pointers.

### call — deliver as rvalue vs. lvalue

| Policy | rvalue-origin stored object | lvalue-origin stored object |
|--------|-----------------------------|-----------------------------|
| `move` | cast to an rvalue            | used as an lvalue           |
| `copy` | used as an lvalue            | used as an lvalue           |

The call policy governs **stored arguments and the functor itself**, including
selection of its `operator() &` or `operator() &&`. Casting to an rvalue does not
itself move anything; consumption depends on the invoked function. Likewise,
`copy` supplies lvalues rather than making copies: the function can mutate or move
from them. Neither policy guarantees that repeating a call preserves its result
or remains valid.

Fresh call-time arguments are always perfect-forwarded, regardless of the call
policy; the function must accept the resulting argument types and categories.

## Usage

`carry::policy<...>` is a value-less bundle; bind it to a name and reuse it:

```cpp
namespace cs = carry::storage;
namespace cc = carry::call;
namespace ct = carry::target;

using owning  = carry::policy<cs::by_value,     cc::copy, ct::by_value>;
using piping  = carry::policy<cs::as_passed,    cc::move, ct::by_value>;
using viewing = carry::policy<cs::by_reference, cc::copy, ct::by_reference>;

auto f = owning::carry(fn, a, b);   // stores decayed copies of fn, a, b
f(c);
f(d);                             // valid if fn and the stored state permit reuse
```

- **Partial application** of any arity, including all-or-none:
  ```cpp
  owning::carry(add, 1, 2, 3)();    // all bound
  owning::carry(add)(1, 2, 3);      // none bound
  ```
- **Chaining**: a curried callable is itself a functor.
  ```cpp
  auto g = owning::carry(add, 1);
  auto h = owning::carry(g, 2);
  h(3);                             // 6
  ```
- **References** survive value storage via `std::reference_wrapper`:
  ```cpp
  int acc = 0;
  owning::carry(accumulate, std::ref(acc), 5)();   // mutates acc
  ```

## Choosing a policy

- **Own stored values**: `by_value, copy, by_value` — stores arguments and the
  functor by value and uses them as lvalues. References inside them remain borrowed.
- **Pipelines / sinks**: `as_passed, move, by_value` — owns rvalue arguments and
  presents them as rvalues on invocation; borrows lvalue arguments.
- **Continuations / immediate use**: `by_reference, …` — zero-copy, but the
  callable must not outlive what it borrows.

## Lifetime caveats

- **Ownership is shallow.** Storing a pointer, `std::string_view`, `std::span`,
  `std::reference_wrapper`, or a lambda with reference captures by value does not
  extend the lifetime of its referents. Keep those referents alive and valid for
  every call that accesses them.
- **`by_reference` does not extend lifetimes.** A temporary passed directly to
  `carry` normally dies at the end of that full-expression, not when `carry`
  returns. Immediate invocation within the same expression can be valid; a later
  call through the stored reference is not. `std::move(x)` instead refers to the
  existing object `x`: its lifetime is unchanged, and borrowing it does not itself
  move from it. This applies to borrowed functors too.
- **`as_passed` owns rvalue arguments and borrows lvalue arguments.** An owned
  value can still contain borrowed references; an lvalue referent must remain
  alive and valid while used by the callable.
- **`std::ref(x)` explicitly borrows `x`.** Value storage preserves the wrapper;
  moving that wrapper under `call::move` does not itself move from `x`. The invoked
  function can still mutate or move from `x`.

The Debug sanitizer preset runs tests under AddressSanitizer + UBSan. These can
detect lifetime violations exercised by tests, but do not prove lifetime safety.

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
add_subdirectory(Carry)
target_link_libraries(your_target PRIVATE carry::carry)
```

## Layout

```
include/carry.hpp            the library (Doxygen-documented public interface)
tests/unit/carry/            value-category & lifetime unit tests
docs/coverage-proof.md       formal argument that the suite covers every case
cmake/, CMakePresets.json    build system
```

See [docs/coverage-proof.md](docs/coverage-proof.md) for a standard-referenced
proof that the unit tests exercise the complete space of value-category cases
and their combinations.
