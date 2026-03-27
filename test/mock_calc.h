#include "gmock/gmock.h"
#include "../src/calc.h"

class MockCalc : public Calc {
public:
    MOCK_METHOD(int, add, (int a, int b), (override));
    MOCK_METHOD(int, mul, (int a, int b), (override));
};