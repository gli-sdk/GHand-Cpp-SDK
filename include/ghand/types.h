#ifndef GHAND_TYPES_H_
#define GHAND_TYPES_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "export.h"

namespace ghand {

// ===== 手部类型定义 =====
enum class HandType : uint8_t {
  NONE,
  LEFT,
  RIGHT,
  NUM_HANDS  // 仅用于计数，不是有效值
};

/**
 * @brief 获取手部类型的字符串表示（用于调试/日志）
 * @warning 不应在实时控制循环中调用（有字符串处理开销）
 */
std::string GHAND_API ToString(HandType type);

// ===== 手指类型定义 =====
enum class FingerType : uint8_t {
  THUMB,
  FF,
  MF,
  RF,
  LF,
  NUM_FINGERS  // 仅用于计数，不是有效值
};

/**
 * @brief 获取手指类型的字符串表示（用于调试/日志）
 * @warning 不应在实时控制循环中调用（有字符串处理开销）
 */
std::string GHAND_API ToString(FingerType finger);

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
  STOPPED = 0,            // 停止
  RUNNING = 1,            // 正常运行中
  ABNORMAL_RUNNING = 2,   // 异常运行
  PROTECTIVE_STOPPED = 3  // 保护性停止
};

/**
 * @brief 获取状态的字符串表示（用于调试/日志）
 * @warning 不应在实时控制循环中调用（有字符串处理开销）
 */
std::string GHAND_API ToString(State state);

enum class ErrorCode : uint8_t {
  NORMAL = 0,
  // 电机错误
  MOTOR_HARDWARE_OVERCURRENT = 1,  // 电机硬件过流
  MOTOR_SOFTWARE_OVERCURRENT = 2,  // 电机软件过流
  MOTOR_BUS_OVERCURRENT = 3,       // 电机母线过流
  MOTOR_PHASE_LOST = 4,            // 电机缺相
  MOTOR_STALLED = 5,               // 电机堵转
  MOTOR_DRIVER_OVERTEMP = 6,       // 电机驱动芯片过温
  MOTOR_COMM_ERROR = 7,            // 电机通信错误
  // 手指错误
  JOINT_CONFLICT = 11,  // 关节冲突
  TIP_CONFLICT = 12,    // 指尖冲突
  // 手部错误
  LOW_TEMP = 21,      // 温度过低
  HIGH_TEMP = 22,     // 温度过高
  LOW_VOLTAGE = 23,   // 电压过低
  HIGH_VOLTAGE = 24,  // 电压过高
  // 触觉传感器错误
  TACTILE_ERROR = 31,  // 触觉传感器错误
  // 数据处理错误
  PARAM_ERROR = 101,  // 参数错误
  TIMEOUT = 102,      // 超时
  // 其他
  UNKNOWN_ERROR = 201  // 未知错误
};

/**
 * @brief 获取错误码的字符串表示（用于调试/日志）
 * @warning 不应在实时控制循环中调用（有字符串处理开销）
 */
std::string GHAND_API ToString(ErrorCode error);

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
 * @brief 获取关节ID的字符串表示（用于调试/日志）
 * @warning 不应在实时控制循环中调用（有字符串处理开销）
 */
std::string GHAND_API ToString(JointId id);

// ===== 产品类型定义 =====
enum class ProductType : uint8_t { G5, AUTO };

/**
 * @brief 获取产品类型的字符串表示（用于调试/日志）
 * @warning 不应在实时控制循环中调用（有字符串处理开销）
 */
std::string GHAND_API ToString(ProductType type);

// ===== 通信类型定义 =====
enum class CommType : uint8_t { ETHERCAT, CANFD, RS485 };

// ===== 控制模式定义 =====
enum class ControlMode : uint8_t { POSITION = 0, TORQUE = 1, SPEED = 2 };

// ===== 设备信息结构 =====
struct DeviceInfo {
  std::string device_name;
  std::string hardware_version;
  std::string software_version;
  std::string motor_driver_version;
  unsigned int serial_number;
};

/**
 * @brief 手部状态数据结构
 */
struct HandState {
  State state;
  ErrorCode error;
  int16_t temperature;
};

/**
 * @brief 单个触觉区域的传感器数据
 */
struct RegionTactile {
  const char* region_name;                // 区域名称（由设备端提供）
  bool state;                             // 传感器状态 (true=正常, false=异常)
  Force resultant_force;                  // 合力数据
  std::vector<Force> distributed_forces;  // 分布力数据
};

/**
 * @brief 触觉数据结构
 *
 * 设计说明：
 * - regions 按设备协议定义的顺序排列，与协议帧字节序一致
 * - region_name 指向设备返回的字符串，生命周期与 TactileData 数据帧相同
 * - sensor_state 按位编码：bit 0-4 分别对应区域 0-4
 * - sensor_error 为全局错误码
 */
struct TactileData {
  uint8_t sensor_state;  // 保留原始字节（调试用）
  uint8_t sensor_error;  // 全局错误码

  std::vector<RegionTactile> regions;
};

// 关节命令结构体，用于参数化关节控制
struct JointCommand {
  JointId id;       // 关节标识符
  float angle;      // 目标角度（deg）
  int8_t velocity;  // 目标速度（-100~100%，根据控制模式）
  int8_t torque;    // 目标力矩（-100~100%，根据控制模式）
};

struct Joint {
  JointId id;
  State state;
  ErrorCode error;
  float angle;
  int8_t velocity;
  int8_t torque;
};

// ===== HandState / Joint 查询函数 =====

inline bool IsNormal(const HandState& hs) {
  return (hs.state == State::STOPPED || hs.state == State::RUNNING) &&
         hs.error == ErrorCode::NORMAL;
}

inline bool HasError(const HandState& hs) {
  return hs.error != ErrorCode::NORMAL ||
         (hs.state != State::RUNNING && hs.state != State::STOPPED);
}

inline bool IsNormal(const Joint& j) {
  return (j.state == State::STOPPED || j.state == State::RUNNING) &&
         j.error == ErrorCode::NORMAL;
}

inline bool HasError(const Joint& j) {
  return j.error != ErrorCode::NORMAL ||
         (j.state != State::RUNNING && j.state != State::STOPPED);
}

std::string GHAND_API ToString(const HandState& hs);
std::string GHAND_API ToString(const Joint& joint);

}  // namespace ghand

#endif  // GHAND_TYPES_H_
