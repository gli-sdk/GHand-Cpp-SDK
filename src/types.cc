// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include "ghand/types.h"

#include <iomanip>
#include <iterator>
#include <sstream>

namespace ghand {

namespace {
std::string FormatBytes(const std::array<uint8_t, 6>& bytes) {
  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < bytes.size(); ++i) {
    if (i > 0) oss << ", ";
    oss << "0x" << std::hex << std::uppercase << std::setw(2)
        << std::setfill('0') << static_cast<int>(bytes[i]) << std::dec;
  }
  oss << "]";
  return oss.str();
}

const char* DescribeHandWarn(size_t byte_index, size_t bit_index) {
  if (byte_index == 0 && bit_index == 0) return "Temperature sensor 1 abnormal";
  if (byte_index == 0 && bit_index == 1) return "Temperature sensor 2 abnormal";
  if (byte_index == 0 && bit_index == 2) return "Fan abnormal";
  return "Reserved";
}

const char* DescribeHandFault(size_t byte_index, size_t bit_index) {
  static const char* kFaults[44] = {
      "Joint motor fault - Thumb2",
      "Joint motor fault - Thumb3",
      "Joint motor fault - Thumb4",
      "Joint motor fault - Thumb5",
      "Joint motor fault - FF2",
      "Joint motor fault - FF3",
      "Joint motor fault - FF4",
      "Joint motor fault - MF2",
      "Joint motor fault - MF3",
      "Joint motor fault - RF2",
      "Joint motor fault - RF3",
      "Joint motor fault - LF2",
      "Joint motor fault - LF3",
      "Thumb tactile sensor abnormal",
      "Index finger tactile sensor abnormal",
      "Middle finger tactile sensor abnormal",
      "Ring finger tactile sensor abnormal",
      "Little finger tactile sensor abnormal",
      "Position sensor abnormal - Thumb2",
      "Position sensor abnormal - Thumb3",
      "Position sensor abnormal - Thumb4",
      "Position sensor abnormal - Thumb5",
      "Position sensor abnormal - FF2",
      "Position sensor abnormal - FF3",
      "Position sensor abnormal - FF4",
      "Position sensor abnormal - MF2",
      "Position sensor abnormal - MF3",
      "Position sensor abnormal - RF2",
      "Position sensor abnormal - RF3",
      "Position sensor abnormal - LF2",
      "Position sensor abnormal - LF3",
      "Position sensor uncalibrated - Thumb2",
      "Position sensor uncalibrated - Thumb3",
      "Position sensor uncalibrated - Thumb4",
      "Position sensor uncalibrated - Thumb5",
      "Position sensor uncalibrated - FF2",
      "Position sensor uncalibrated - FF3",
      "Position sensor uncalibrated - FF4",
      "Position sensor uncalibrated - MF2",
      "Position sensor uncalibrated - MF3",
      "Position sensor uncalibrated - RF2",
      "Position sensor uncalibrated - RF3",
      "Position sensor uncalibrated - LF2",
      "Position sensor uncalibrated - LF3",
  };
  const size_t index = byte_index * 8 + bit_index;
  return index < std::size(kFaults) ? kFaults[index] : "Reserved";
}

const char* DescribeHandCritical(size_t byte_index, size_t bit_index) {
  if (byte_index == 0 && bit_index == 0) return "Unknown error";
  if (byte_index == 0 && bit_index == 1) return "Self-test error";
  if (byte_index == 0 && bit_index == 2) return "Position sensor version mismatch";
  if (byte_index == 0 && bit_index == 3) return "Tactile sensor version mismatch";
  if (byte_index == 0 && bit_index == 4) return "Motor driver version mismatch";
  return "Reserved";
}

void AppendActiveHandErrors(std::ostringstream& oss, const char* level,
                            const std::array<uint8_t, 6>& bytes,
                            const char* (*describe)(size_t, size_t)) {
  for (size_t byte_index = 0; byte_index < bytes.size(); ++byte_index) {
    for (size_t bit_index = 0; bit_index < 8; ++bit_index) {
      if ((bytes[byte_index] & (1u << bit_index)) != 0) {
        oss << ", " << level << "[" << byte_index << "]:bit" << bit_index
            << "=" << describe(byte_index, bit_index);
      }
    }
  }
}
}  // namespace

// ProductType
const char* ToString(ProductType type) {
  switch (type) {
    case ProductType::GHand5:
      return "G5-13A18D-S01";
    case ProductType::AUTO:
      return "AUTO";
    case ProductType::GHandLite1:
      return "L1-Hand-V1";
    default:
      return "UNKNOWN";
  }
}

// HandType
const char* ToString(HandType type) {
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
const char* ToString(FingerType finger) {
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
const char* ToString(State state) {
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
const char* ToString(ErrorCode error) {
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
    case ErrorCode::JOINT_CONFLICT:
      return "JOINT_CONFLICT";
    case ErrorCode::TIP_CONFLICT:
      return "TIP_CONFLICT";
    case ErrorCode::JOINT_POSITION_ERROR:
      return "JOINT_POSITION_ERROR";
    case ErrorCode::LOW_TEMP:
      return "LOW_TEMP";
    case ErrorCode::HIGH_TEMP:
      return "HIGH_TEMP";
    case ErrorCode::LOW_VOLTAGE:
      return "LOW_VOLTAGE";
    case ErrorCode::HIGH_VOLTAGE:
      return "HIGH_VOLTAGE";
    case ErrorCode::TACTILE_DISCONNECTED:
      return "TACTILE_DISCONNECTED";
    case ErrorCode::TACTILE_DATA_ERROR:
      return "TACTILE_DATA_ERROR";
    case ErrorCode::SELF_CHECK_ERROR:
      return "SELF_CHECK_ERROR";
    case ErrorCode::PARAM_ERROR:
      return "PARAM_ERROR";
    case ErrorCode::UNKNOWN_ERROR:
      return "UNKNOWN_ERROR";
    default:
      return "UNKNOWN";
  }
}

const char* ToString(JointStateCode state) {
  switch (state) {
    case JointStateCode::DISABLED:
      return "DISABLED";
    case JointStateCode::RUNNING:
      return "RUNNING";
    case JointStateCode::STOPPED:
      return "STOPPED";
    case JointStateCode::FAULT:
      return "FAULT";
    default:
      return "UNKNOWN_STATE";
  }
}

const char* ToString(JointErrorCode error) {
  switch (error) {
    case JointErrorCode::NORMAL:
      return "NORMAL";
    case JointErrorCode::MOTOR_HARDWARE_OVERCURRENT:
      return "MOTOR_HARDWARE_OVERCURRENT";
    case JointErrorCode::MOTOR_CONTINUOUS_CURRENT_HIGH:
      return "MOTOR_CONTINUOUS_CURRENT_HIGH";
    case JointErrorCode::MOTOR_BUS_OVERCURRENT:
      return "MOTOR_BUS_OVERCURRENT";
    case JointErrorCode::MOTOR_HARDWARE_OVERVOLTAGE:
      return "MOTOR_HARDWARE_OVERVOLTAGE";
    case JointErrorCode::MOTOR_HARDWARE_UNDERVOLTAGE:
      return "MOTOR_HARDWARE_UNDERVOLTAGE";
    case JointErrorCode::MOTOR_PHASE_LOSS:
      return "MOTOR_PHASE_LOSS";
    case JointErrorCode::MOTOR_STALLED:
      return "MOTOR_STALLED";
    case JointErrorCode::MOTOR_CHIP_OVERTEMPERATURE:
      return "MOTOR_CHIP_OVERTEMPERATURE";
    case JointErrorCode::COMMUNICATION_LOST:
      return "COMMUNICATION_LOST";
    case JointErrorCode::MOTOR_SOFTWARE_OVERVOLTAGE:
      return "MOTOR_SOFTWARE_OVERVOLTAGE";
    case JointErrorCode::MOTOR_SOFTWARE_UNDERVOLTAGE:
      return "MOTOR_SOFTWARE_UNDERVOLTAGE";
    case JointErrorCode::MOTOR_OVERTEMPERATURE:
      return "MOTOR_OVERTEMPERATURE";
    default:
      return "UNKNOWN_ERROR";
  }
}

const char* ToString(CommType type) {
  switch (type) {
    case CommType::ETHERCAT:
      return "ethercat";
    case CommType::CANFD:
      return "canfd";
    case CommType::RS485:
      return "rs485";
    default:
      return "unknown";
  }
}

const char* ToString(SdkError error) {
  switch (error) {
    case SdkError::OK:
      return "OK";
    case SdkError::NOT_CONNECTED:
      return "NOT_CONNECTED";
    case SdkError::INVALID_ARGUMENT:
      return "INVALID_ARGUMENT";
    case SdkError::NOT_SUPPORTED:
      return "NOT_SUPPORTED";
    case SdkError::TIMEOUT:
      return "TIMEOUT";
    case SdkError::TRANSPORT_ERROR:
      return "TRANSPORT_ERROR";
    case SdkError::PROTOCOL_ERROR:
      return "PROTOCOL_ERROR";
    case SdkError::CONFIG_ERROR:
      return "CONFIG_ERROR";
    case SdkError::DEVICE_REJECTED:
      return "DEVICE_REJECTED";
    case SdkError::INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    default:
      return "UNKNOWN";
  }
}

// JointId
const char* ToString(JointId id) {
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
      << ", error_warn=" << FormatBytes(hs.error_warn)
      << ", error_fault=" << FormatBytes(hs.error_fault)
      << ", error_critical=" << FormatBytes(hs.error_critical)
      << ", temperature=" << hs.temperature;
  AppendActiveHandErrors(oss, "warn", hs.error_warn, DescribeHandWarn);
  AppendActiveHandErrors(oss, "fault", hs.error_fault, DescribeHandFault);
  AppendActiveHandErrors(oss, "critical", hs.error_critical,
                         DescribeHandCritical);
  oss << "}";
  return oss.str();
}

// Joint
std::string ToString(const Joint& joint) {
  std::ostringstream oss;
  oss << "Joint[" << ghand::ToString(joint.id)
      << "]{state=" << ghand::ToString(joint.state)
      << ", error=" << ghand::ToString(joint.error)
      << ", mode=" << static_cast<int>(joint.mode)
      << ", stop=" << static_cast<int>(joint.stop)
      << ", angle=" << joint.angle
      << ", velocity=" << static_cast<int>(joint.velocity)
      << ", current=" << joint.current << "}";
  return oss.str();
}

}  // namespace ghand
