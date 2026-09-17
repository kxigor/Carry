#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "carry.hpp"

namespace {

using owning = carry::policy<carry::storage::by_value, carry::call::copy,
                             carry::target::by_value>;
using borrowing = carry::policy<carry::storage::by_reference, carry::call::copy,
                                carry::target::by_reference>;

// Check the desired contract at runtime so unsupported const calls report a
// test failure instead of preventing the rest of the suite from building.
template <typename Curried, typename... Args>
void ExpectConstCall(const Curried& curried, int expected, Args&&... args) {
  if constexpr (std::is_invocable_v<const Curried&, Args...>) {
    EXPECT_EQ(curried(std::forward<Args>(args)...), expected);
  } else {
    ADD_FAILURE()
        << "The stored callable and arguments support this const call, "
           "but the returned closure does not.";
  }
}

}  // namespace

TEST(ConstInvocation, CallableWithoutBoundArguments) {
  const auto curried = owning::carry([](int value) { return value + 1; });
  ExpectConstCall(curried, 8, 7);
}

TEST(ConstInvocation, CallableWithOwnedArguments) {
  const auto curried =
      owning::carry([](int left, int right) { return left + right; }, 10);
  ExpectConstCall(curried, 13, 3);
}

TEST(ConstInvocation, BorrowedObjectsCanBeMutatedThroughConstClosure) {
  int value = 10;
  auto increment = [calls = 0](int& number) mutable {
    number += ++calls;
    return number;
  };
  const auto curried = borrowing::carry(increment, value);

  ExpectConstCall(curried, 11);
  ExpectConstCall(curried, 13);
}

TEST(ConstInvocation, NonConstClosureStillSupportsMutableOwnedFunctor) {
  auto curried = owning::carry(
      [total = 0](int delta) mutable { return total += delta; }, 2);

  EXPECT_EQ(curried(), 2);
  EXPECT_EQ(curried(), 4);
}

// These assertions describe the required result types. They deliberately fail
// when either forwarding lambda deduces auto instead of decltype(auto).
TEST(ReturnType, PreservesLvalueReferenceAndMutation) {
  int value = 10;
  auto curried = owning::carry([](int& number) -> int& { return number; },
                               std::ref(value));

  ASSERT_TRUE((std::is_same_v<decltype(curried()), int&>));
  auto&& result = curried();
  EXPECT_EQ(std::addressof(result), std::addressof(value));
  result = 42;
  EXPECT_EQ(value, 42);
}

TEST(ReturnType, PreservesConstLvalueReference) {
  const int value = 10;
  auto curried =
      owning::carry([](const int& number) -> const int& { return number; });

  ASSERT_TRUE((std::is_same_v<decltype(curried(value)), const int&>));
  auto&& result = curried(value);
  EXPECT_EQ(std::addressof(result), std::addressof(value));
}

TEST(ReturnType, PreservesRvalueReferenceWithoutMovingFromReferent) {
  auto value = std::make_unique<int>(42);
  auto* const pointee = value.get();
  auto curried = owning::carry(
      [](std::unique_ptr<int>& pointer) -> std::unique_ptr<int>&& {
        return std::move(pointer);
      },
      std::ref(value));

  EXPECT_TRUE((std::is_same_v<decltype(curried()), std::unique_ptr<int>&&>));
  // Binding an rvalue reference must not move-construct another unique_ptr.
  auto&& result = curried();
  EXPECT_EQ(std::addressof(result), std::addressof(value));
  EXPECT_EQ(result.get(), pointee);
  EXPECT_EQ(value.get(), pointee);
}

TEST(ReturnType, ReferenceToOwnedArgumentRemainsValidWhileClosureLives) {
  auto curried = owning::carry([](int& number) -> int& { return number; }, 10);

  ASSERT_TRUE((std::is_same_v<decltype(curried()), int&>));
  auto&& first = curried();
  first = 42;
  auto&& second = curried();
  EXPECT_EQ(std::addressof(first), std::addressof(second));
  EXPECT_EQ(second, 42);
}

TEST(ReturnType, PreservesArrayReference) {
  int values[] = {1, 2, 3};
  auto curried = owning::carry([&values]() -> int (&)[3] { return values; });

  ASSERT_TRUE((std::is_same_v<decltype(curried()), int (&)[3]>));
  auto&& result = curried();
  EXPECT_EQ(std::addressof(result[0]), std::addressof(values[0]));
}

TEST(ReturnType, ChainedClosuresPreserveReference) {
  int value = 10;
  auto first = owning::carry(
      [](int& number, int delta) -> int& {
        number += delta;
        return number;
      },
      std::ref(value));
  auto second = owning::carry(std::move(first), 5);

  ASSERT_TRUE((std::is_same_v<decltype(second()), int&>));
  auto&& result = second();
  EXPECT_EQ(std::addressof(result), std::addressof(value));
  EXPECT_EQ(value, 15);
}

TEST(ReturnType, MoveOnlyValueIsReturnedByValue) {
  auto curried = owning::carry([] { return std::make_unique<int>(42); });

  ASSERT_TRUE((std::is_same_v<decltype(curried()), std::unique_ptr<int>>));
  auto result = curried();
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, 42);
}

TEST(ReturnType, NonMovableValueUsesGuaranteedCopyElision) {
  struct Value {
    explicit Value(int number) : number(number) {}
    Value(const Value&) = delete;
    Value(Value&&) = delete;
    int number;
  };
  auto curried = owning::carry([] { return Value{42}; });

  ASSERT_TRUE((std::is_same_v<decltype(curried()), Value>));
  auto result = curried();
  EXPECT_EQ(result.number, 42);
}
