#include "xiaoyao/types.h"
#include <sstream>

namespace xiaoyao {

// ===== ProductType =====
std::string ToString(ProductType type) {
    switch (type) {
        case ProductType::GHAND:      return "GHAND";
        case ProductType::GHAND_LITE: return "GHAND_LITE";
        default:                      return "UNKNOWN";
    }
}

// ===== HandType =====
std::string ToString(HandType type) {
    switch (type) {
        case HandType::NONE:  return "NONE";
        case HandType::LEFT:  return "LEFT";
        case HandType::RIGHT: return "RIGHT";
        default:              return "UNKNOWN";
    }
}

// ===== FingerType =====
std::string ToString(FingerType finger) {
    switch (finger) {
        case FingerType::THUMB: return "THUMB";
        case FingerType::FF:    return "INDEX";
        case FingerType::MF:    return "MIDDLE";
        case FingerType::RF:    return "RING";
        case FingerType::LF:    return "PINKY";
        default:                return "UNKNOWN";
    }
}

// ===== State =====
std::string ToString(State state) {
    switch (state) {
        case State::STOPPED:           return "STOPPED";
        case State::RUNNING:           return "RUNNING";
        case State::ABNORMAL_RUNNING:  return "ABNORMAL_RUNNING";
        case State::PROTECTIVE_STOPPED: return "PROTECTIVE_STOPPED";
        default:                       return "UNKNOWN";
    }
}

// ===== ErrorCode =====
std::string ToString(ErrorCode error) {
    switch (error) {
        case ErrorCode::NORMAL:                      return "NORMAL";
        case ErrorCode::MOTOR_HARDWARE_OVERCURRENT:  return "MOTOR_HARDWARE_OVERCURRENT";
        case ErrorCode::MOTOR_SOFTWARE_OVERCURRENT:  return "MOTOR_SOFTWARE_OVERCURRENT";
        case ErrorCode::MOTOR_BUS_OVERCURRENT:       return "MOTOR_BUS_OVERCURRENT";
        case ErrorCode::MOTOR_PHASE_LOST:            return "MOTOR_PHASE_LOST";
        case ErrorCode::MOTOR_STALLED:               return "MOTOR_STALLED";
        case ErrorCode::MOTOR_DRIVER_OVERTEMP:       return "MOTOR_DRIVER_OVERTEMP";
        case ErrorCode::MOTOR_COMM_ERROR:            return "MOTOR_COMM_ERROR";
        case ErrorCode::JOINT_CONFLICT:      return "JOINT_CONFLICT";
        case ErrorCode::TIP_CONFLICT:        return "TIP_CONFLICT";
        case ErrorCode::LOW_TEMP:            return "LOW_TEMP";
        case ErrorCode::HIGH_TEMP:           return "HIGH_TEMP";
        case ErrorCode::LOW_VOLTAGE:         return "LOW_VOLTAGE";
        case ErrorCode::HIGH_VOLTAGE:        return "HIGH_VOLTAGE";
        case ErrorCode::TACTILE_ERROR:       return "TACTILE_ERROR";
        case ErrorCode::PARAM_ERROR:         return "PARAM_ERROR";
        case ErrorCode::TIMEOUT:             return "TIMEOUT";
        case ErrorCode::UNKNOWN_ERROR:       return "UNKNOWN_ERROR";
        default:                             return "UNKNOWN";
    }
}

// ===== JointId =====
std::string ToString(JointId id) {
    switch (id) {
        case JointId::THUMB_DIP:        return "THUMB_DIP";
        case JointId::THUMB_PIP:        return "THUMB_PIP";
        case JointId::THUMB_MCP:        return "THUMB_MCP";
        case JointId::THUMB_SWING:      return "THUMB_SWING";
        case JointId::THUMB_ROTATION:   return "THUMB_ROTATION";
        case JointId::FF_DIP:           return "INDEX_DIP";
        case JointId::FF_PIP:           return "INDEX_PIP";
        case JointId::FF_MCP:           return "INDEX_MCP";
        case JointId::FF_SWING:         return "INDEX_SWING";
        case JointId::MF_DIP:           return "MIDDLE_DIP";
        case JointId::MF_PIP:           return "MIDDLE_PIP";
        case JointId::MF_MCP:           return "MIDDLE_MCP";
        case JointId::RF_DIP:           return "RING_DIP";
        case JointId::RF_PIP:           return "RING_PIP";
        case JointId::RF_MCP:           return "RING_MCP";
        case JointId::LF_DIP:           return "PINKY_DIP";
        case JointId::LF_PIP:           return "PINKY_PIP";
        case JointId::LF_MCP:           return "PINKY_MCP";
        default:                        return "UNKNOWN";
    }
}

// ===== HandState =====
std::string HandState::ToString() const {
    std::ostringstream oss;
    oss << "HandState{state=" << xiaoyao::ToString(state)
        << ", error=" << xiaoyao::ToString(error)
        << ", temperature=" << temperature << "}";
    return oss.str();
}

// ===== Joint =====
std::string Joint::ToString() const {
    std::ostringstream oss;
    oss << "Joint[" << xiaoyao::ToString(id)
        << "]{state=" << xiaoyao::ToString(state)
        << ", error=" << xiaoyao::ToString(error)
        << ", angle=" << angle
        << ", velocity=" << static_cast<int>(velocity)
        << ", torque=" << static_cast<int>(torque) << "}";
    return oss.str();
}

}  // namespace xiaoyao
