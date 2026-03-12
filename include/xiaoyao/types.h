#ifndef XIAOYAO_TYPES_H_
#define XIAOYAO_TYPES_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace xiaoyao {

// ===== 手部类型定义 =====
enum class HandType : uint8_t {
    NONE,
    LEFT,
    RIGHT,
    NUM_HANDS  // 仅用于计数，不是有效值
};

// ===== 手指类型定义 =====
enum class FingerType : uint8_t {
    THUMB,
    FF,
    MF,
    RF,
    LF,
    NUM_FINGERS  // 仅用于计数，不是有效值
};

// ===== 力数据结构 =====
struct Force {
    float x;
    float y;
    float z;
    Force() : x(0.0f), y(0.0f), z(0.0f) {}
    Force(int16_t x_val, int16_t y_val, uint16_t z_val)
        : x(x_val), y(y_val), z(z_val) {}
};

enum class State : uint8_t {
    STOPPED = 0,          // 停止
    RUNNING = 1,          // 运行中
    ABNORMAL_RUNNING = 2,  // 异常运行
    PROTECTIVE_STOP = 3    // 保护性停止
};

enum class ErrorCode : uint8_t {
    NORMAL = 0,
    // 电机错误
    MOTOR_HARDWARE_OVERCURRENT = 1,   // 电机硬件过流
    MOTOR_SOFTWARE_OVERCURRENT = 2,   // 电机软件过流
    MOTOR_BUS_OVERCURRENT = 3,        // 电机母线过流
    MOTOR_PHASE_LOST = 4,             // 电机缺相
    MOTOR_STALLED = 5,                // 电机堵转
    MOTOR_DRIVER_OVERTEMP = 6,        // 电机驱动芯片过温
    MOTOR_COMM_ERROR = 7,             // 电机通信错误
    // 手指错误
    JOINT_CONFLICT = 11,              // 关节冲突
    TIP_CONFLICT = 12,                // 指尖冲突
    // 手部错误
    LOW_TEMP = 21,                    // 温度过低
    HIGH_TEMP = 22,                   // 温度过高
    LOW_VOLTAGE = 23,                 // 电压过低
    HIGH_VOLTAGE = 24,                // 电压过高
    // 触觉传感器错误
    TACTILE_ERROR = 31,               // 触觉传感器错误
    // 数据处理错误
    PARAM_ERROR = 101,                // 参数错误
    TIMEOUT = 102,                    // 超时
    // 其他
    UNKNOWN_ERROR = 201               // 未知错误
};

enum class ForceType : uint8_t {
    RESULTANT = 0,  // 合力
    SAMPLE = 1      // 分布力
};

enum class JointId : uint8_t {
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
    State state;
    ErrorCode error;
    int16_t temperature;

    // 便利方法
    bool IsNormal() const {
        return (state == State::STOPPED || state == State::RUNNING) && error == ErrorCode::NORMAL;
    }

    bool HasError() const {
        return error != ErrorCode::NORMAL || state != State::RUNNING || state == State::STOPPED;
    }

    std::string ToString() const;
};

/**
 * @brief 触觉数据结构
 */
struct TactileData {
    /**
     * @brief 每个手指的合力数据
     * 索引对应 FingerType：0=THUMB, 1=FF, 2=MF, 3=RF, 4=LF
     */
    std::array<Force, static_cast<int>(FingerType::NUM_FINGERS)> resultants;

    /**
     * @brief 每个手指的分布力数据
     * 索引对应 FingerType：0=THUMB, 1=FF, 2=MF, 3=RF, 4=LF
     */
    std::array<std::vector<Force>, static_cast<int>(FingerType::NUM_FINGERS)> samples;

    // 便利方法
    /**
     * @brief 获取指定手指的合力
     * @param finger 手指类型
     * @return 合力数据
     */
    Force GetResultant(FingerType finger) const {
        return resultants[static_cast<int>(finger)];
    }

    /**
     * @brief 获取指定手指的分布力
     * @param finger 手指类型
     * @return 分布力数据（传感器数组）
     */
    const std::vector<Force>& GetSamples(FingerType finger) const {
        return samples[static_cast<int>(finger)];
    }

    /**
     * @brief 检查是否包含有效数据
     * @return true 表示至少有一个力值不为0
     */
    bool HasData() const {
        for (const Force& f : resultants) {
            if (f.x != 0.0f || f.y != 0.0f || f.z != 0.0f) {
                return true;
            }
        }

        for (const auto& sample_vec : samples) {
            if (!sample_vec.empty()) {
                return true;
            }
        }

        return false;
    }

    std::string ToString() const;
};

// 关节命令结构体，用于参数化关节控制
struct JointCommand {
    JointId id;       // 关节标识符
    float angle;      // 目标角度
    uint8_t velocity; // 目标速度
    uint8_t torque;   // 目标力矩
};

struct Joint {
    JointId id;
    State state;
    ErrorCode error;
    float angle;
    uint8_t velocity;
    uint8_t torque;

    // 便利方法
    bool IsNormal() const {
        return (state == State::STOPPED || state == State::RUNNING) && error == ErrorCode::NORMAL;
    }

    bool HasError() const {
        return error != ErrorCode::NORMAL || state != State::RUNNING || state == State::STOPPED;
    }

    std::string ToString() const;
};

// ===== 枚举工具函数 =====
const char* ToString(JointId id);
const char* ToString(FingerType type);
const char* ToString(State state);
const char* ToString(ErrorCode error);
const char* ToString(ForceType force_type);
const char* ToString(HandType type);

}  // namespace xiaoyao

#endif  // XIAOYAO_TYPES_H_
