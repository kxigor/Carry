#pragma once

/// @file curry.hpp
/// @brief Header-only function currying (partial application) with explicit,
///        composable control over value categories and lifetimes.
///
/// Currying is configured by three orthogonal policy axes, bundled into
/// @ref curry::policy "policy":
///   - **storage** — how curried arguments are kept (own vs. borrow);
///   - **call**    — how stored arguments are delivered to the functor;
///   - **target**  — how the functor itself is kept.
///
/// @code
/// using fast = curry::policy<curry::storage::by_value,
///                            curry::call::move,
///                            curry::target::by_value>;
/// auto add   = [](int a, int b, int c) { return a + b + c; };
/// auto add12 = fast::curry(add, 1, 2);
/// int  six   = add12(3);  // 1 + 2 + 3
/// @endcode

#include <tuple>
#include <type_traits>
#include <utility>

namespace curry {

// ============================================================================

/// @brief Argument-storage policies: how curried arguments become the tuple
///        held by the returned callable.
///
/// Each policy exposes a single static @c store. It receives the arguments as
/// lvalues and re-applies their original value category through the explicit
/// @c OriginalTypes pack, so the *element types* of the resulting tuple are
/// what distinguishes the policies.
namespace storage {

/// @brief Own every argument by value (decay-copy rvalues, copy lvalues).
///
/// Stored values are independent of the caller, so the resulting callable is
/// reusable and never dangles. @c std::decay_t is used rather than
/// @c std::make_tuple deliberately: @c make_tuple *unwraps*
/// @c std::reference_wrapper into a bare reference, which @c call::move would
/// then move *through*, stealing from the caller's object. @c decay keeps the
/// wrapper as an owned value handle, so @c std::ref threads a reference safely.
struct by_value {
  /// @tparam OriginalTypes Original (possibly reference) argument types, given
  ///                       explicitly to recover their value category.
  /// @tparam LvalueTypes   Deduced lvalue parameter types.
  /// @param  args          The arguments, bound as lvalues.
  /// @return A tuple of decayed, owned values.
  template <typename... OriginalTypes, typename... LvalueTypes>
  static auto store(LvalueTypes&... args) {
    return std::tuple<std::decay_t<OriginalTypes>...>(
        std::forward<OriginalTypes>(args)...);
  }
};

/// @brief Own rvalues by value, borrow lvalues by reference ("as passed").
///
/// Mirrors how the caller supplied each argument: temporaries are moved in and
/// owned; named objects are referenced. Borrowed lvalues must outlive the
/// callable.
struct as_passed {
  /// @copydoc by_value::store
  template <typename... OriginalTypes, typename... LvalueTypes>
  static auto store(LvalueTypes&... args) {
    return std::tuple<OriginalTypes...>(std::forward<OriginalTypes>(args)...);
  }
};

/// @brief Borrow every argument by reference.
///
/// Nothing is copied or moved. Storing a reference to a temporary dangles once
/// currying returns, so use only with lvalues that outlive the callable.
struct by_reference {
  /// @copydoc by_value::store
  template <typename... OriginalTypes, typename... LvalueTypes>
  static auto store(LvalueTypes&... args) {
    return std::forward_as_tuple(std::forward<OriginalTypes>(args)...);
  }
};

}  // namespace storage

// ============================================================================

/// @brief Functor-storage policies.
///
/// The functor is held as a one-element tuple, so the @ref curry::storage
/// mechanism applies verbatim. The names are re-exported for intent:
/// @c target::by_value owns the functor, @c target::by_reference borrows it,
/// @c target::as_passed mirrors how it was supplied.
namespace target {

using storage::as_passed;
using storage::by_reference;
using storage::by_value;

}  // namespace target

// ============================================================================

/// @brief Call policies: how a *stored* value (a curried argument or the
///        functor) is delivered to the functor at invocation time.
///
/// Note this governs only stored values. Fresh call-time arguments are always
/// perfect-forwarded, regardless of the call policy.
namespace call {

/// @brief Move a stored value into the functor iff its origin was an rvalue.
///
/// An rvalue origin is owned and is cast to an rvalue (moved in), making the
/// call single-shot for that argument. An lvalue origin is only borrowed and is
/// passed through unchanged.
struct move {
  /// @tparam OriginalType Original argument type; only its value category
  ///                      (lvalue- vs. non-lvalue-reference) is consulted.
  /// @tparam LvalueType   Deduced type of the stored object.
  /// @param  arg          The stored object, as an lvalue.
  /// @return @p arg as an rvalue when the origin was an rvalue, else as lvalue.
  template <typename OriginalType, typename LvalueType>
  static decltype(auto) forward(LvalueType& arg) {
    // Decide from the origin's value *category*, not its exact type: storage
    // may have decayed it (array -> pointer, reference_wrapper -> T&), so
    // std::forward<OriginalType> would static_cast between unrelated types.
    if constexpr (std::is_lvalue_reference_v<OriginalType>) {
      return arg;
    } else {
      return std::move(arg);
    }
  }
};

/// @brief Always deliver a stored value as an lvalue (never move it).
///
/// Storage is left intact, so the callable is reusable. This also downgrades
/// fresh rvalue call-time arguments to lvalues.
struct copy {
  /// @tparam OriginalType Original argument type (unused; copy ignores it).
  /// @tparam LvalueType   Deduced type of the stored object.
  /// @param  arg          The stored object, as an lvalue.
  /// @return @p arg as an lvalue.
  template <typename OriginalType, typename LvalueType>
  static decltype(auto) forward(LvalueType& arg) {
    return arg;
  }
};

}  // namespace call

// ============================================================================

/// @cond INTERNAL
namespace detail {
template <typename T>
struct is_tuple : std::false_type {};
template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};
}  // namespace detail
/// @endcond

/// @brief Constrains the @c Storage and @c Target parameters of
///        @ref curry::policy.
///
/// Satisfied by a type with a static @c store mapping arguments to a
/// @c std::tuple — i.e. the @ref curry::storage policies and their
/// @ref curry::target re-exports.
template <typename P>
concept storage_policy = requires(int lvalue) {
  { P::template store<int&>(lvalue) };
  requires detail::is_tuple<decltype(P::template store<int&>(lvalue))>::value;
};

/// @brief Constrains the @c Call parameter of @ref curry::policy.
///
/// Satisfied by a type with a static @c forward that delivers a stored value —
/// i.e. the @ref curry::call policies.
template <typename P>
concept call_policy =
    requires(int lvalue) { P::template forward<int&>(lvalue); };

// ============================================================================

/// @brief A reusable currying configuration: one bundle of the three policy
///        axes, invoked through @ref policy::curry.
///
/// @tparam Storage Argument-storage policy (models @ref storage_policy).
/// @tparam Call    Delivery policy (models @ref call_policy).
/// @tparam Target  Functor-storage policy (models @ref storage_policy).
///
/// @code
/// using owning = curry::policy<curry::storage::by_value,
///                              curry::call::copy,
///                              curry::target::by_value>;
/// auto greet = owning::curry(fn, a, b);
/// @endcode
template <storage_policy Storage, call_policy Call, storage_policy Target>
struct policy {
  /// @brief Curry @p functor with leading @p curried_args.
  ///
  /// @tparam Functor     The callable, of any value category.
  /// @tparam CurriedArgs The leading arguments to bind now.
  /// @param  functor      Callable to partially apply (kept per @c Target).
  /// @param  curried_args Arguments bound now (kept per @c Storage).
  /// @return A callable that, given the remaining arguments, invokes
  ///         @p functor with the bound arguments (delivered per @c Call)
  ///         followed by the new ones (always perfect-forwarded).
  // clang-format off
  template <typename Functor, typename... CurriedArgs>
  static auto curry(Functor&& functor, CurriedArgs&&... curried_args) {
    return [
      functor               = Target::template store<Functor>(functor),
      curried_args_as_tuple = Storage::template store<CurriedArgs...>(curried_args...)
    ]
    <typename... OtherArgs>(OtherArgs&&... other_args) mutable {
      return std::apply(
        [&](auto&... stored_curried_args) {
          return Call::template forward<Functor>(std::get<0>(functor))(
            Call::template forward<CurriedArgs>(stored_curried_args)...,
            std::forward<OtherArgs>(other_args)...
          );
        },
        curried_args_as_tuple
      );
    };
  }
  // clang-format on
};

}  // namespace curry
