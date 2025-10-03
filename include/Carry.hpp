#pragma once

#include <tuple>
#include <utility>

namespace Carry {
template <typename Functor, typename... CurriedArgs>
auto Carry(Functor&& functor, CurriedArgs&&... curried_args) {
  return [functor = std::forward<Functor>(functor),
          curried_args_as_tuple =
              std::tuple<CurriedArgs...>{std::forward<CurriedArgs>(
                  curried_args)...}]<typename... OtherArgs>(
             OtherArgs&&... other_args) mutable {
    return std::apply(
        [&]<typename... ForwardCurriedArgs>(
            ForwardCurriedArgs&&... forwarded_curried_args) {
          return std::forward<Functor>(functor)(
              std::forward<CurriedArgs>(forwarded_curried_args)...,
              std::forward<OtherArgs>(other_args)...);
        },
        curried_args_as_tuple);
  };
}
}  // namespace Carry
