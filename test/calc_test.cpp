#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "../src/calc.h"
#include "mock_calc.h"

using ::testing::Return;

// GTest 基础测试
TEST(CalcTest, AddTest) {
    Calc calc;
    EXPECT_EQ(calc.add(2, 3), 5);
    ASSERT_EQ(calc.add(0, 0), 0);
}

// GMock 测试
TEST(CalcMockTest, MockAddTest) {
    MockCalc mock;
    EXPECT_CALL(mock, add(2, 3)).WillOnce(Return(10));
    EXPECT_EQ(mock.add(2, 3), 10);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}