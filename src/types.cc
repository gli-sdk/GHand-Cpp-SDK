#include "xiaoyao/types.h"
#include <map>
#include <sstream>

namespace xiaoyao {

// ===== 数据结构便利方法实现 =====

std::string HandTemperature::ToString() const {
    std::ostringstream oss;
    oss << "HandTemperature{state=" << xiaoyao::ToString(state)
        << ", error=" << xiaoyao::ToString(error)
        << ", temperature=" << temperature << "}";
    return oss.str();
}

std::string Joint::ToString() const {
    std::ostringstream oss;
    oss << "Joint{id=" << xiaoyao::ToString(id)
        << ", state=" << xiaoyao::ToString(state)
        << ", error=" << xiaoyao::ToString(error)
        << ", angle=" << angle
        << ", velocity=" << static_cast<int>(velocity)
        << ", torque=" << static_cast<int>(torque) << "}";
    return oss.str();
}

// ===== ToString 函数实现 =====

namespace {
    // 关节ID映射
    const std::map<JointId, const char*> kJointIdNames = {
        {JointId::THUMB_DIP, "THUMB_DIP"},
        {JointId::THUMB_PIP, "THUMB_PIP"},
        {JointId::THUMB_MCP, "THUMB_MCP"},
        {JointId::THUMB_SWING, "THUMB_SWING"},
        {JointId::THUMB_ROTATION, "THUMB_ROTATION"},
        {JointId::FF_DIP, "FF_DIP"},
        {JointId::FF_PIP, "FF_PIP"},
        {JointId::FF_MCP, "FF_MCP"},
        {JointId::FF_SWING, "FF_SWING"},
        {JointId::MF_DIP, "MF_DIP"},
        {JointId::MF_PIP, "MF_PIP"},
        {JointId::MF_MCP, "MF_MCP"},
        {JointId::RF_DIP, "RF_DIP"},
        {JointId::RF_PIP, "RF_PIP"},
        {JointId::RF_MCP, "RF_MCP"},
        {JointId::LF_DIP, "LF_DIP"},
        {JointId::LF_PIP, "LF_PIP"},
        {JointId::LF_MCP, "LF_MCP"},
        {JointId::NUM_JOINTS, "NUM_JOINTS"}
    };

    // 状态映射
    const std::map<State, const char*> kStateNames = {
        {State::STOPPED, "STOPPED"},
        {State::RUNNING, "RUNNING"},
        {State::ABNORMAL_RUNNING, "ABNORMAL_RUNNING"},
        {State::PROTECTIVE_STOP, "PROTECTIVE_STOP"}
    };

    // 错误码映射
    const std::map<ErrorCode, const char*> kErrorCodeNames = {
        {ErrorCode::NORMAL, "NORMAL"},
        {ErrorCode::MOTOR_HARDWARE_OVERCURRENT, "MOTOR_HARDWARE_OVERCURRENT"},
        {ErrorCode::MOTOR_SOFTWARE_OVERCURRENT, "MOTOR_SOFTWARE_OVERCURRENT"},
        {ErrorCode::MOTOR_BUS_OVERCURRENT, "MOTOR_BUS_OVERCURRENT"},
        {ErrorCode::MOTOR_PHASE_LOST, "MOTOR_PHASE_LOST"},
        {ErrorCode::MOTOR_STALLED, "MOTOR_STALLED"},
        {ErrorCode::MOTOR_DRIVER_OVERTEMP, "MOTOR_DRIVER_OVERTEMP"},
        {ErrorCode::MOTOR_COMM_ERROR, "MOTOR_COMM_ERROR"},
        {ErrorCode::JOINT_CONFLICT, "JOINT_CONFLICT"},
        {ErrorCode::TIP_CONFLICT, "TIP_CONFLICT"},
        {ErrorCode::LOW_TEMP, "LOW_TEMP"},
        {ErrorCode::HIGH_TEMP, "HIGH_TEMP"},
        {ErrorCode::LOW_VOLTAGE, "LOW_VOLTAGE"},
        {ErrorCode::HIGH_VOLTAGE, "HIGH_VOLTAGE"},
        {ErrorCode::TACTILE_ERROR, "TACTILE_ERROR"},
        {ErrorCode::PARAM_ERROR, "PARAM_ERROR"},
        {ErrorCode::TIMEOUT, "TIMEOUT"},
        {ErrorCode::UNKNOWN_ERROR, "UNKNOWN_ERROR"}
    };

    // 手指类型映射
    const std::map<FingerType, const char*> kFingerTypeNames = {
        {FingerType::THUMB, "THUMB"},
        {FingerType::FF, "FF"},
        {FingerType::MF, "MF"},
        {FingerType::RF, "RF"},
        {FingerType::LF, "LF"},
        {FingerType::NUM_FINGERS, "NUM_FINGERS"}
    };

    // 力类型映射
    const std::map<ForceType, const char*> kForceTypeNames = {
        {ForceType::RESULTANT, "RESULTANT"},
        {ForceType::SAMPLE, "SAMPLE"}
    };

    // 手部类型映射
    const std::map<xiaoyao::HandType, const char*> kHandTypeNames = {
        {xiaoyao::HandType::NONE, "NONE"},
        {xiaoyao::HandType::LEFT, "LEFT"},
        {xiaoyao::HandType::RIGHT, "RIGHT"},
        {xiaoyao::HandType::NUM_HANDS, "NUM_HANDS"}
    };
}

const char* ToString(FingerType type) {
    auto it = kFingerTypeNames.find(type);
    return (it != kFingerTypeNames.end()) ? it->second : "UNKNOWN";
}

const char* ToString(JointId id) {
    auto it = kJointIdNames.find(id);
    return (it != kJointIdNames.end()) ? it->second : "UNKNOWN";
}

const char* ToString(State state) {
    auto it = kStateNames.find(state);
    return (it != kStateNames.end()) ? it->second : "UNKNOWN";
}

const char* ToString(ErrorCode error) {
    auto it = kErrorCodeNames.find(error);
    return (it != kErrorCodeNames.end()) ? it->second : "UNKNOWN";
}

const char* ToString(ForceType type) {
    auto it = kForceTypeNames.find(type);
    return (it != kForceTypeNames.end()) ? it->second : "UNKNOWN";
}

const char* ToString(HandType type) {
    auto it = kHandTypeNames.find(type);
    return (it != kHandTypeNames.end()) ? it->second : "UNKNOWN";
}

}  // namespace xiaoyao
