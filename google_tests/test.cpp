#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "Carry.hpp"

class MoveTracker {
 public:
  static int move_constructions;
  static int copy_constructions;

  static void reset() {
    move_constructions = 0;
    copy_constructions = 0;
  }

  MoveTracker() = default;

  MoveTracker(const MoveTracker&) { ++copy_constructions; }

  MoveTracker(MoveTracker&&) noexcept { ++move_constructions; }

  MoveTracker& operator=(const MoveTracker&) {
    ++copy_constructions;
    return *this;
  }

  MoveTracker& operator=(MoveTracker&&) noexcept {
    ++move_constructions;
    return *this;
  }
};

int MoveTracker::move_constructions = 0;
int MoveTracker::copy_constructions = 0;

auto add_numbers = [](int a, int b, int c) { return a + b + c; };

auto concat_strings = [](const std::string& a, std::string&& b,
                         const std::string& c) { return a + b + c; };

auto process_movable = [](MoveTracker&& m1, const MoveTracker& m2,
                          MoveTracker&& m3) {
  std::ignore = m1, std::ignore = m2, std::ignore = m3;
  return MoveTracker::move_constructions;
};

static std::string create_string();
static std::string create_string() { return "rvalue"; }

TEST(CarryTest, BasicFunctionality) {
  auto curried = Carry::Carry(add_numbers, 1, 2);
  EXPECT_EQ(curried(3), 6);
  EXPECT_EQ(curried(10), 13);
}

TEST(CarryTest, LvalueArguments) {
  int a = 5, b = 10;
  auto curried = Carry::Carry(add_numbers, a, b);

  int c = 15;
  EXPECT_EQ(curried(c), 30);

  EXPECT_EQ(curried(c), 30);
}

TEST(CarryTest, RvalueReferences) {
  std::string str1 = "hello";
  std::string str2 = "world";

  auto curried = Carry::Carry(concat_strings, str1, std::move(str2), "!");

  EXPECT_EQ(curried(), "helloworld!");
}

TEST(CarryTest, MixedValueCategories) {
  std::string lvalue_str = "lvalue";

  auto curried =
      Carry::Carry(concat_strings, lvalue_str, create_string(), "end");

  EXPECT_EQ(curried(), "lvaluervalueend");
}

TEST(CarryTest, MoveSemantics) {
  MoveTracker::reset();

  MoveTracker m1, m2, m3;
  auto curried =
      Carry::Carry(process_movable, std::move(m1), m2, std::move(m3));

  int copy_before = MoveTracker::copy_constructions;
  curried();
  int copy_after = MoveTracker::copy_constructions;

  EXPECT_EQ(copy_before, copy_after);
}

TEST(CarryTest, PrvalueArguments) {
  auto curried = Carry::Carry(add_numbers, 100, 200);
  EXPECT_EQ(curried(300), 600);

  auto string_curried =
      Carry::Carry(concat_strings, "start", std::string("middle"), "end");
  EXPECT_EQ(string_curried(), "startmiddleend");
}

TEST(CarryTest, XvalueArguments) {
  std::string xvalue_str = "xvalue";

  auto curried =
      Carry::Carry([](std::string&& a, std::string&& b) { return a + b; },
                   std::move(xvalue_str), "suffix");

  EXPECT_EQ(curried(), "xvaluesuffix");
}

TEST(CarryTest, MultipleCalls) {
  auto curried = Carry::Carry(add_numbers, 1, 2);

  EXPECT_EQ(curried(3), 6);
  EXPECT_EQ(curried(4), 7);
  EXPECT_EQ(curried(5), 8);
}

TEST(CarryTest, ChainingCarry) {
  auto curried1 = Carry::Carry(add_numbers, 1);
  auto curried2 = Carry::Carry(curried1, 2);

  EXPECT_EQ(curried2(3), 6);
}

TEST(CarryTest, EmptyCurriedArgs) {
  auto curried = Carry::Carry(add_numbers);
  EXPECT_EQ(curried(1, 2, 3), 6);
}

TEST(CarryTest, AllCurriedArgs) {
  auto curried = Carry::Carry(add_numbers, 1, 2, 3);
  EXPECT_EQ(curried(), 6);
}

TEST(CarryTest, ReferencePreservation) {
  int value = 10;

  auto modifier = [](int& a, int b) {
    a += b;
    return a;
  };

  auto curried = Carry::Carry(modifier, std::ref(value), 5);

  EXPECT_EQ(curried(), 15);
  EXPECT_EQ(value, 15);
}

TEST(CarryTest, ConstArguments) {
  const int const_val = 100;

  auto curried = Carry::Carry(add_numbers, const_val, 200);
  EXPECT_EQ(curried(300), 600);
}

TEST(CarryTest, FunctionObjects) {
  struct Multiplier {
    int factor;
    int operator()(int a, int b) const { return a * b * factor; }
  };

  Multiplier multiplier{2};
  auto curried = Carry::Carry(multiplier, 3);
  EXPECT_EQ(curried(4), 24);
}

TEST(CarryTest, TemporaryOptimization) {
  auto complex_operation = [](std::vector<int>&& vec, int multiplier) {
    return static_cast<int>(vec.size()) * multiplier;
  };

  auto curried =
      Carry::Carry(complex_operation, std::vector<int>{1, 2, 3, 4, 5}, 2);
  EXPECT_EQ(curried(), 10);
}

TEST(CarryTest, PerfectForwarding) {
  auto forward_test = [](auto&&... args) { return sizeof...(args); };

  std::string str = "test";
  auto curried = Carry::Carry(forward_test, 1, std::move(str), "literal");
  EXPECT_EQ(curried(4.0, 'c'), 5);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}