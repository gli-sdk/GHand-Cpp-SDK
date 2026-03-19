#include <gtest/gtest.h>
#include <gmock/gmock.h>

// 待Mock的接口
class Database {
public:
    virtual ~Database() = default;
    virtual int getValue(const std::string& key) = 0;
};

// Mock类
class MockDatabase : public Database {
public:
    MOCK_METHOD(int, getValue, (const std::string& key), (override));
};

// 待测试类
class Calculator {
private:
    Database* db_;
public:
    Calculator(Database* db) : db_(db) {}
    int calculateTotal(const std::string& key) {
        int base = db_->getValue(key);
        return base * 2 + 10;
    }
};

// 测试用例
TEST(CalculatorTest, CalculateTotal_ValidValue) {
    MockDatabase mock_db;
    // 设置Mock行为
    EXPECT_CALL(mock_db, getValue("test_key"))
        .Times(1)
        .WillOnce(testing::Return(20));
    
    Calculator calc(&mock_db);
    EXPECT_EQ(calc.calculateTotal("test_key"), 50);
}

// 测试入口
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}