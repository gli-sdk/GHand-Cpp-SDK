#include "ghand/types.h"

#include <sstream>

namespace ghand {

// ProductType
std::string ToString(ProductType type) {
  switch (type) {
    case ProductType::G5:
      return "G5-13A23D-S01";
    case ProductType::AUTO:
      return "AUTO";
    case ProductType::L1:
      return "L1-Hand-V1";
    default:
      return "UNKNOWN";
  }
}

// HandType
std::string ToString(HandType type) {
  switch (type) {
    case HandType::NONE:
      return "NONE";
    case HandType::LEFT:
      return "LEFT";
    case HandType::RIGHT:
      return "RIGHT";
    default:
      return "UNKNOWN";
  }
}

// FingerType
std::string ToString(FingerType finger) {
  switch (finger) {
    case FingerType::THUMB:
      return "THUMB";
    case FingerType::FF:
      return "FF";
    case FingerType::MF:
      return "MF";
    case FingerType::RF:
      return "RF";
    case FingerType::LF:
      return "LF";
    default:
      return "UNKNOWN";
  }
}

// State
std::string ToString(State state) {
  switch (state) {
    case State::STOPPED:
      return "STOPPED";
    case State::RUNNING:
      return "RUNNING";
    case State::ABNORMAL_RUNNING:
      return "ABNORMAL_RUNNING";
    case State::PROTECTIVE_STOPPED:
      return "PROTECTIVE_STOPPED";
    default:
      return "UNKNOWN";
  }
}

// ErrorCode
std::string ToString(ErrorCode error) {
  switch (error) {
    case ErrorCode::NORMAL:
      return "NORMAL";
    case ErrorCode::MOTOR_HARDWARE_OVERCURRENT:
      return "MOTOR_HARDWARE_OVERCURRENT";
    case ErrorCode::MOTOR_SOFTWARE_OVERCURRENT:
      return "MOTOR_SOFTWARE_OVERCURRENT";
    case ErrorCode::MOTOR_BUS_OVERCURRENT:
      return "MOTOR_BUS_OVERCURRENT";
    case ErrorCode::MOTOR_PHASE_LOST:
      return "MOTOR_PHASE_LOST";
    case ErrorCode::MOTOR_STALLED:
      return "MOTOR_STALLED";
    case ErrorCode::MOTOR_DRIVER_OVERTEMP:
      return "MOTOR_DRIVER_OVERTEMP";
    case ErrorCode::MOTOR_COMM_ERROR:
      return "MOTOR_COMM_ERROR";
    case ErrorCode::MOTOR_OVER_TEMP:
      return "MOTOR_OVER_TEMP";
    case ErrorCode::MOTOR_DRIVER_OVER_TEMP:
      return "MOTOR_DRIVER_OVER_TEMP";
    case ErrorCode::JOINT_CONFLICT:
      return "JOINT_CONFLICT";
    case ErrorCode::TIP_CONFLICT:
      return "TIP_CONFLICT";
    case ErrorCode::LOW_TEMP:
      return "LOW_TEMP";
    case ErrorCode::HIGH_TEMP:
      return "HIGH_TEMP";
    case ErrorCode::LOW_VOLTAGE:
      return "LOW_VOLTAGE";
    case ErrorCode::HIGH_VOLTAGE:
      return "HIGH_VOLTAGE";
    case ErrorCode::TACTILE_ERROR:
      return "TACTILE_ERROR";
    case ErrorCode::PARAM_ERROR:
      return "PARAM_ERROR";
    case ErrorCode::TIMEOUT:
      return "TIMEOUT";
    case ErrorCode::UNKNOWN_ERROR:
      return "UNKNOWN_ERROR";
    default:
      return "UNKNOWN";
  }
}

// JointId
std::string ToString(JointId id) {
  switch (id) {
    case JointId::THUMB_IP:
      return "THUMB_IP";
    case JointId::THUMB_MCP:
      return "THUMB_MCP";
    case JointId::THUMB_TMC_FE:
      return "THUMB_TMC_FE";
    case JointId::THUMB_TMC_AA:
      return "THUMB_TMC_AA";
    case JointId::THUMB_TMC_PS:
      return "THUMB_TMC_PS";
    case JointId::FF_DIP:
      return "FF_DIP";
    case JointId::FF_PIP:
      return "FF_PIP";
    case JointId::FF_MCP:
      return "FF_MCP";
    case JointId::FF_MCP_AA:
      return "FF_MCP_AA";
    case JointId::MF_DIP:
      return "MF_DIP";
    case JointId::MF_PIP:
      return "MF_PIP";
    case JointId::MF_MCP:
      return "MIDDLE_MCP";
    case JointId::RF_DIP:
      return "RF_DIP";
    case JointId::RF_PIP:
      return "RF_PIP";
    case JointId::RF_MCP:
      return "RF_MCP";
    case JointId::LF_DIP:
      return "LF_DIP";
    case JointId::LF_PIP:
      return "LF_PIP";
    case JointId::LF_MCP:
      return "LF_MCP";
    default:
      return "UNKNOWN";
  }
}

// HandState
std::string ToString(const HandState& hs) {
  std::ostringstream oss;
  oss << "HandState{state=" << ghand::ToString(hs.state)
      << ", error=" << ghand::ToString(hs.error)
      << ", temperature=" << hs.temperature << "}";
  return oss.str();
}

// Joint
std::string ToString(const Joint& joint) {
  std::ostringstream oss;
  oss << "Joint[" << ghand::ToString(joint.id)
      << "]{state=" << ghand::ToString(joint.state)
      << ", error=" << ghand::ToString(joint.error)
      << ", angle=" << joint.angle
      << ", velocity=" << static_cast<int>(joint.velocity)
      << ", torque=" << static_cast<int>(joint.torque) << "}";
  return oss.str();
}

}  // namespace ghand
