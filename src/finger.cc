#include "xiaoyao/finger.h"

#include <cstdint>
#include <cstring>
#include <vector>

// 实现虚函数，根据不同手指类型返回不同的偏移量和尺寸
int Finger::GetResultantForceOffset() const {
    // 根据不同手指类型返回对应的偏移量
    // 从dexhand.cc中的注释可以看到偏移量数组: int offset[5] = {230, 392, 491, 590, 689};
    switch (type_) {
        case THUMB:
            return 150;  // 大拇指的偏移量
        case FF:         // 食指
            return 312;
        case MF:  // 中指
            return 411;
        case RF:  // 无名指
            return 510;
        case LF:  // 小指
            return 609;
        default:
            return 0;
    }
}
int Finger::GetSampleForcesOffset() const {
    // 根据不同手指类型返回样本力数据的偏移量
    // 这里使用默认实现，实际值需要根据具体协议定义
    switch (type_) {
        case THUMB:
            return 156;  // 假设在结果力数据之后
        case FF:
            return 318;
        case MF:
            return 417;
        case RF:
            return 516;
        case LF:
            return 615;
        default:
            return 0;
    }
}

int Finger::GetResultantForceSize() const {
    // ResultantForce的大小为6字节 (int16_t x + int16_t y + uint16_t z)
    return 6;
}

int Finger::GetSampleForcesSize() const {
    int sample_force_size = 0;
    // 返回样本力数据的总大小，这里返回0表示可变大小
    // 实际大小需要根据协议定义
    if (type_ == THUMB) {
        sample_force_size = 52;
    } else {
        sample_force_size = 31;
    }
    return sample_force_size;
}

Force Finger::GetResultantForce(const uint8_t* data, int data_size) const {
    Force resultant;
    resultant.x = 0.0f;
    resultant.y = 0.0f;
    resultant.z = 0.0f;

    // 检查数据是否有效以及大小是否足够
    if (data != nullptr && data_size >= 6) {
        int16_t raw_x = static_cast<int16_t>(data[0] | (data[1] << 8));
        // x值为有符号值，取低8位并转换为float
        resultant.x = static_cast<float>(static_cast<int8_t>(raw_x & 0xFF) * 0.1f);

        // 处理y值 (int16_t) - 小端模式读取，取低8位，乘以0.1
        int16_t raw_y = static_cast<int16_t>(data[2] | (data[3] << 8));
        // y值为有符号值，取低8位并转换为float
        resultant.y = static_cast<float>(static_cast<int8_t>(raw_y & 0xFF) * 0.1f);

        // 处理z值 (uint16_t) - 小端模式读取，取低8位，乘以0.1
        uint16_t raw_z = static_cast<uint16_t>(data[4] | (data[5] << 8));
        // z值为无符号值，取低8位并转换为float
        resultant.z = static_cast<float>(static_cast<uint8_t>(raw_z & 0xFF) * 0.1f);
    }

    return resultant;
}

std::vector<Force> Finger::GetSampleForces(const uint8_t* data, int data_size) const {
    std::vector<Force> forces;
    // 清空之前的数据
    forces.clear();

    if (data != nullptr && data_size > 0) {
        // 解析每个样本力数据
        for (int i = 0; i < data_size; i++) {
            Force sample_force;

            // 解析x值 - 有符号8位值，转换为float并乘以0.1
            sample_force.x = static_cast<float>(static_cast<int8_t>(data[0 + i * 3]) * 0.1f);

            // 解析y值 - 有符号8位值，转换为float并乘以0.1
            sample_force.y = static_cast<float>(static_cast<int8_t>(data[1 + i * 3]) * 0.1f);

            // 解析z值 - 无符号8位值，转换为float并乘以0.1
            sample_force.z = static_cast<float>(static_cast<uint8_t>(data[2 + i * 3]) * 0.1f);

            forces.push_back(sample_force);
        }
    }
    return forces;
}
