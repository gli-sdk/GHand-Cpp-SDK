#ifndef XIAOYAO_TYPES_H_
#define XIAOYAO_TYPES_H_

#include <cstdint>
#include <vector>

// 前向声明
enum JointId {
    THUMB_DIP,
    THUMB_PIP,
    THUMB_MCP,
    THUMB_SWING,
    THUMB_ROTATION,
    FF_DIP,
    FF_PIP,
    FF_MCP,
    FF_SWING,
    MF_DIP,
    MF_PIP,
    MF_MCP,
    RF_DIP,
    RF_PIP,
    RF_MCP,
    LF_DIP,
    LF_PIP,
    LF_MCP,
    NUM_JOINTS
};

/**
 * @brief 手部温度数据结构
 */
struct HandTemperature {
    uint8_t state;
    uint8_t error;
    int16_t temperature;
};

struct MotionParam {
    float angle;
    uint8_t velocity;
    uint8_t torque;
};

struct JointState {
    uint8_t state;
    uint8_t error;
    float angle;
    uint8_t velocity;
    uint8_t torque;
};



// 关节命令结构体，用于参数化关节控制
struct JointCommand {
    JointId id;              // 关节标识符
    MotionParam target;      // 目标参数（angle, velocity, torque）
};

struct Joint {
    JointId id;
    JointState state;
};

#endif  // XIAOYAO_TYPES_H_
