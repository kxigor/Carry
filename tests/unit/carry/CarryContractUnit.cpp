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
template <typename Carried, typename... Args>
void ExpectConstCall(const Carried& carried, int expected, Args&&... args) {
  if constexpr (std::is_invocable_v<const Carried&, Args...>) {
    EXPECT_EQ(carried(std::forward<Args>(args)...), expected);
  } else {
    ADD_FAILURE()
        << "The stored callable and arguments support this const call, "
           "but the returned closure does not.";
  }
}

}  // namespace

TEST(ConstInvocation, CallableWithoutBoundArguments) {
  const auto carried = owning::carry([](int value) { return value + 1; });
  ExpectConstCall(carried, 8, 7);
}

TEST(ConstInvocation, CallableWithOwnedArguments) {
  const auto carried =
      owning::carry([](int left, int right) { return left + right; }, 10);
  ExpectConstCall(carried, 13, 3);
}

TEST(ConstInvocation, BorrowedObjectsCanBeMutatedThroughConstClosure) {
  int value = 10;
  auto increment = [calls = 0](int& number) mutable {
    number += ++calls;
    return number;
  };
  const auto carried = borrowing::carry(increment, value);

  ExpectConstCall(carried, 11);
  ExpectConstCall(carried, 13);
}

TEST(ConstInvocation, NonConstClosureStillSupportsMutableOwnedFunctor) {
  auto carried = owning::carry(
      [total = 0](int delta) mutable { return total += delta; }, 2);

  EXPECT_EQ(carried(), 2);
  EXPECT_EQ(carried(), 4);
}

// These assertions describe the required result types. They deliberately fail
// when either forwarding lambda deduces auto instead of decltype(auto).
TEST(ReturnType, PreservesLvalueReferenceAndMutation) {
  int value = 10;
  auto carried = owning::carry([](int& number) -> int& { return number; },
                               std::ref(value));

  ASSERT_TRUE((std::is_same_v<decltype(carried()), int&>));
  auto&& result = carried();
  EXPECT_EQ(std::addressof(result), std::addressof(value));
  result = 42;
  EXPECT_EQ(value, 42);
}

TEST(ReturnType, PreservesConstLvalueReference) {
  const int value = 10;
  auto carried =
      owning::carry([](const int& number) -> const int& { return number; });

  ASSERT_TRUE((std::is_same_v<decltype(carried(value)), const int&>));
  auto&& result = carried(value);
  EXPECT_EQ(std::addressof(result), std::addressof(value));
}

TEST(ReturnType, PreservesRvalueReferenceWithoutMovingFromReferent) {
  auto value = std::make_unique<int>(42);
  auto* const pointee = value.get();
  auto carried = owning::carry(
      [](std::unique_ptr<int>& pointer) -> std::unique_ptr<int>&& {
        return std::move(pointer);
      },
      std::ref(value));

  EXPECT_TRUE((std::is_same_v<decltype(carried()), std::unique_ptr<int>&&>));
  // Binding an rvalue reference must not move-construct another unique_ptr.
  auto&& result = carried();
  EXPECT_EQ(std::addressof(result), std::addressof(value));
  EXPECT_EQ(result.get(), pointee);
  EXPECT_EQ(value.get(), pointee);
}

TEST(ReturnType, ReferenceToOwnedArgumentRemainsValidWhileClosureLives) {
  auto carried = owning::carry([](int& number) -> int& { return number; }, 10);

  ASSERT_TRUE((std::is_same_v<decltype(carried()), int&>));
  auto&& first = carried();
  first = 42;
  auto&& second = carried();
  EXPECT_EQ(std::addressof(first), std::addressof(second));
  EXPECT_EQ(second, 42);
}

TEST(ReturnType, PreservesArrayReference) {
  int values[] = {1, 2, 3};
  auto carried = owning::carry([&values]() -> int (&)[3] { return values; });

  ASSERT_TRUE((std::is_same_v<decltype(carried()), int (&)[3]>));
  auto&& result = carried();
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
  auto carried = owning::carry([] { return std::make_unique<int>(42); });

  ASSERT_TRUE((std::is_same_v<decltype(carried()), std::unique_ptr<int>>));
  auto result = carried();
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
  auto carried = owning::carry([] { return Value{42}; });

  ASSERT_TRUE((std::is_same_v<decltype(carried()), Value>));
  auto result = carried();
  EXPECT_EQ(result.number, 42);
}
