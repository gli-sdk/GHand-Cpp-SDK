#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "../src/calc.h"
#include "mock_calc.h"

using ::testing::Return;

// GTest 基础测试
TEST(CalcTest, AddTest) {
    RecordProperty("NodeID1", "node-001");
    RecordProperty("Priority1", "high");
    RecordProperty("Description1", "测试加法运算的边界条件");
    Calc calc;
    EXPECT_EQ(calc.add(2, 3), 5);
    ASSERT_EQ(calc.add(0, 0), 0);
}


// node:001 description:GMock 测试
TEST(CalcMockTest, MockAddTest) {
    RecordProperty("NodeID", "node-001");
    RecordProperty("Priority", "high");
    RecordProperty("Description", "测试加法运算的边界条件");
    MockCalc mock;
    EXPECT_CALL(mock, add(2, 3)).WillOnce(Return(10));
    EXPECT_EQ(mock.add(2, 3), 10);
}

// int main(int argc, char **argv) {
//     testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }

int main(int argc, char** argv) {
    std::cout << "GoogleTest version: " 
              << GTEST_VERSION_ << "\n";  // 需要 1.8.0 或更高版本
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}