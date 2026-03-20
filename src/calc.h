// src/calc.h
#pragma once
class Calc {
public:
    virtual int add(int a, int b) { return a + b; }
    virtual int mul(int a, int b) { return a * b; }
    virtual ~Calc() = default;
};