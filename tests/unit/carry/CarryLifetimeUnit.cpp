#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "carry.hpp"

namespace {

using owning = carry::policy<carry::storage::by_value, carry::call::copy,
                             carry::target::by_value>;
using borrowing = carry::policy<carry::storage::by_reference, carry::call::copy,
                                carry::target::by_reference>;
using mixed = carry::policy<carry::storage::as_passed, carry::call::copy,
                            carry::target::by_value>;

TEST(Lifetime, OwnedArgumentAndFunctorSurviveTheirSourceScope) {
  std::weak_ptr<int> argument_lifetime;
  std::weak_ptr<int> functor_lifetime;
  auto curried = [&] {
    auto argument = std::make_shared<int>(7);
    auto captured = std::make_shared<int>(11);
    argument_lifetime = argument;
    functor_lifetime = captured;
    auto function = [captured](const std::shared_ptr<int>& value) {
      return *captured + *value;
    };
    return owning::carry(function, argument);
  }();

  EXPECT_EQ(argument_lifetime.use_count(), 1);
  EXPECT_EQ(functor_lifetime.use_count(), 1);
  EXPECT_EQ(curried(), 18);
}

TEST(Lifetime, CopyingOwnedClosureSeparatesArgumentAndFunctorState) {
  auto function = [calls = 0](int& value) mutable {
    return std::pair{++calls, ++value};
  };
  auto original = owning::carry(function, 10);
  auto copy = original;

  EXPECT_EQ(original(), (std::pair{1, 11}));
  EXPECT_EQ(original(), (std::pair{2, 12}));
  EXPECT_EQ(copy(), (std::pair{1, 11}));
  EXPECT_EQ(copy(), (std::pair{2, 12}));
}

TEST(Lifetime, CopyingBorrowingClosureSharesArgumentAndFunctorState) {
  int value = 10;
  auto function = [calls = 0](int& argument) mutable {
    return std::pair{++calls, ++argument};
  };
  auto original = borrowing::carry(function, value);
  auto copy = original;

  EXPECT_EQ(original(), (std::pair{1, 11}));
  EXPECT_EQ(copy(), (std::pair{2, 12}));
  EXPECT_EQ(value, 12);
  EXPECT_EQ(function(value), (std::pair{3, 13}));
}

TEST(Lifetime, MovingMixedClosurePreservesOwnershipAndBorrowing) {
  int borrowed = 10;
  auto moved = [&borrowed] {
    auto original = mixed::carry(
        [offset = std::make_unique<int>(11)](const std::unique_ptr<int>& owned,
                                             int& referenced) {
          return *offset + *owned + ++referenced;
        },
        std::make_unique<int>(7), borrowed);
    auto destination = std::move(original);
    return destination;
  }();

  EXPECT_EQ(moved(), 29);
  EXPECT_EQ(borrowed, 11);
  borrowed = 20;
  EXPECT_EQ(moved(), 39);
  EXPECT_EQ(borrowed, 21);
}

TEST(Lifetime, BorrowedTemporaryArgumentAndFunctorLastThroughImmediateCall) {
  std::weak_ptr<int> argument_lifetime;
  std::weak_ptr<int> functor_lifetime;
  auto make_resource = [](std::weak_ptr<int>& observer, int value) {
    auto owner = std::make_shared<int>(value);
    observer = owner;
    return owner;
  };

  const int result = borrowing::carry(
      [captured = make_resource(functor_lifetime, 11)](
          const std::shared_ptr<int>& value) { return *captured + *value; },
      make_resource(argument_lifetime, 7))();

  EXPECT_EQ(result, 18);
  EXPECT_TRUE(argument_lifetime.expired());
  EXPECT_TRUE(functor_lifetime.expired());
}

TEST(Lifetime, BorrowingNamedXvalueDoesNotMoveOrEndItsLifetime) {
  auto value = std::make_unique<int>(7);
  auto read = [](const std::unique_ptr<int>& pointer) { return *pointer; };
  auto curried = borrowing::carry(read, std::move(value));

  ASSERT_NE(value, nullptr);
  EXPECT_EQ(curried(), 7);
  *value = 9;
  EXPECT_EQ(curried(), 9);
}

TEST(Lifetime, ValueStoredViewDoesNotOwnItsCharacters) {
  auto owner = std::make_shared<std::string>("borrowed");
  std::weak_ptr<std::string> observer = owner;
  auto curried =
      owning::carry([](std::string_view value) { return value.size(); },
                    std::string_view(*owner));

  EXPECT_EQ(curried(), 8U);
  EXPECT_EQ(observer.use_count(), 1);
  owner.reset();
  EXPECT_TRUE(observer.expired());
  // Keep the closure alive, but never access its expired view.
}

TEST(Lifetime, ValueStoredReferenceWrapperDoesNotExtendReferentLifetime) {
  auto owner = std::make_shared<int>(7);
  std::weak_ptr<int> observer = owner;
  auto curried =
      owning::carry([](const int& value) { return value; }, std::ref(*owner));

  EXPECT_EQ(curried(), 7);
  EXPECT_EQ(observer.use_count(), 1);
  owner.reset();
  EXPECT_TRUE(observer.expired());
  // Destroying a reference wrapper does not access its expired referent.
}

TEST(Lifetime, ValueStoredFunctorDoesNotOwnItsReferenceCapture) {
  std::weak_ptr<int> observer;
  [[maybe_unused]] auto curried = [&] {
    auto owner = std::make_shared<int>(7);
    observer = owner;
    int& value = *owner;
    auto function = [&value] { return value; };
    auto result = owning::carry(function);
    EXPECT_EQ(result(), 7);
    EXPECT_EQ(observer.use_count(), 1);
    return result;
  }();

  EXPECT_TRUE(observer.expired());
  // The stored functor still exists; invoking its reference capture would be
  // UB.
}

}  // namespace
