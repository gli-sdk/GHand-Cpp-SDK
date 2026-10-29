// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include "ghand/types.h"


namespace ghand {

namespace {
template <typename Flags>
Flags MakeHandErrorFlag(size_t bit) {
  Flags result;
  result[bit / 8] = static_cast<uint8_t>(1u << (bit % 8));
  return result;
}
}  // namespace

const HandWarning HandWarning::NONE{};
const HandWarning HandWarning::TEMPERATURE_SENSOR_1_ABNORMAL = MakeHandErrorFlag<HandWarning>(0);
const HandWarning HandWarning::TEMPERATURE_SENSOR_2_ABNORMAL = MakeHandErrorFlag<HandWarning>(1);
const HandWarning HandWarning::FAN_ABNORMAL = MakeHandErrorFlag<HandWarning>(2);
const HandWarning HandWarning::TACTILE_BOARD_COMMUNICATION_ABNORMAL = MakeHandErrorFlag<HandWarning>(3);
const HandWarning HandWarning::THUMB_TACTILE_SENSOR_DISCONNECTED = MakeHandErrorFlag<HandWarning>(4);
const HandWarning HandWarning::FOREFINGER_TACTILE_SENSOR_DISCONNECTED = MakeHandErrorFlag<HandWarning>(5);
const HandWarning HandWarning::MIDDLE_FINGER_TACTILE_SENSOR_DISCONNECTED = MakeHandErrorFlag<HandWarning>(6);
const HandWarning HandWarning::RING_FINGER_TACTILE_SENSOR_DISCONNECTED = MakeHandErrorFlag<HandWarning>(7);
const HandWarning HandWarning::LITTLE_FINGER_TACTILE_SENSOR_DISCONNECTED = MakeHandErrorFlag<HandWarning>(8);
const HandWarning HandWarning::SELF_TEST_ERROR = MakeHandErrorFlag<HandWarning>(9);
HandWarning operator|(const HandWarning& lhs, const HandWarning& rhs) {
  HandWarning result;
  for (size_t i = 0; i < result.size(); ++i) result[i] = lhs[i] | rhs[i];
  return result;
}
HandWarning operator&(const HandWarning& lhs, const HandWarning& rhs) {
  HandWarning result;
  for (size_t i = 0; i < result.size(); ++i) result[i] = lhs[i] & rhs[i];
  return result;
}
const HandFault HandFault::NONE{};
const HandFault HandFault::JOINT_MOTOR_THUMB2 = MakeHandErrorFlag<HandFault>(0);
const HandFault HandFault::JOINT_MOTOR_THUMB3 = MakeHandErrorFlag<HandFault>(1);
const HandFault HandFault::JOINT_MOTOR_THUMB4 = MakeHandErrorFlag<HandFault>(2);
const HandFault HandFault::JOINT_MOTOR_THUMB5 = MakeHandErrorFlag<HandFault>(3);
const HandFault HandFault::JOINT_MOTOR_FF2 = MakeHandErrorFlag<HandFault>(4);
const HandFault HandFault::JOINT_MOTOR_FF3 = MakeHandErrorFlag<HandFault>(5);
const HandFault HandFault::JOINT_MOTOR_FF4 = MakeHandErrorFlag<HandFault>(6);
const HandFault HandFault::JOINT_MOTOR_MF2 = MakeHandErrorFlag<HandFault>(7);
const HandFault HandFault::JOINT_MOTOR_MF3 = MakeHandErrorFlag<HandFault>(8);
const HandFault HandFault::JOINT_MOTOR_RF2 = MakeHandErrorFlag<HandFault>(9);
const HandFault HandFault::JOINT_MOTOR_RF3 = MakeHandErrorFlag<HandFault>(10);
const HandFault HandFault::JOINT_MOTOR_LF2 = MakeHandErrorFlag<HandFault>(11);
const HandFault HandFault::JOINT_MOTOR_LF3 = MakeHandErrorFlag<HandFault>(12);
const HandFault HandFault::POSITION_SENSOR_ABNORMAL_THUMB2 = MakeHandErrorFlag<HandFault>(13);
const HandFault HandFault::POSITION_SENSOR_ABNORMAL_THUMB3 = MakeHandErrorFlag<HandFault>(14);
const HandFault HandFault::POSITION_SENSOR_ABNORMAL_THUMB4 = MakeHandErrorFlag<HandFault>(15);
const HandFault HandFault::POSITION_SENSOR_ABNORMAL_THUMB5 = MakeHandErrorFlag<HandFault>(16);
const HandFault HandFault::POSITION_SENSOR_ABNORMAL_FF2 = MakeHandErrorFlag<HandFault>(17);
const HandFault HandFault::POSITION_SENSOR_ABNORMAL_FF3 = MakeHandErrorFlag<HandFault>(18);
const HandFault HandFault::POSITION_SENSOR_ABNORMAL_FF4 = MakeHandErrorFlag<HandFault>(19);
const HandFault HandFault::POSITION_SENSOR_ABNORMAL_MF2 = MakeHandErrorFlag<HandFault>(20);
const HandFault HandFault::POSITION_SENSOR_ABNORMAL_MF3 = MakeHandErrorFlag<HandFault>(21);
const HandFault HandFault::POSITION_SENSOR_ABNORMAL_RF2 = MakeHandErrorFlag<HandFault>(22);
const HandFault HandFault::POSITION_SENSOR_ABNORMAL_RF3 = MakeHandErrorFlag<HandFault>(23);
const HandFault HandFault::POSITION_SENSOR_ABNORMAL_LF2 = MakeHandErrorFlag<HandFault>(24);
const HandFault HandFault::POSITION_SENSOR_ABNORMAL_LF3 = MakeHandErrorFlag<HandFault>(25);
const HandFault HandFault::POSITION_SENSOR_UNCALIBRATED_THUMB2 = MakeHandErrorFlag<HandFault>(26);
const HandFault HandFault::POSITION_SENSOR_UNCALIBRATED_THUMB3 = MakeHandErrorFlag<HandFault>(27);
const HandFault HandFault::POSITION_SENSOR_UNCALIBRATED_THUMB4 = MakeHandErrorFlag<HandFault>(28);
const HandFault HandFault::POSITION_SENSOR_UNCALIBRATED_THUMB5 = MakeHandErrorFlag<HandFault>(29);
const HandFault HandFault::POSITION_SENSOR_UNCALIBRATED_FF2 = MakeHandErrorFlag<HandFault>(30);
const HandFault HandFault::POSITION_SENSOR_UNCALIBRATED_FF3 = MakeHandErrorFlag<HandFault>(31);
const HandFault HandFault::POSITION_SENSOR_UNCALIBRATED_FF4 = MakeHandErrorFlag<HandFault>(32);
const HandFault HandFault::POSITION_SENSOR_UNCALIBRATED_MF2 = MakeHandErrorFlag<HandFault>(33);
const HandFault HandFault::POSITION_SENSOR_UNCALIBRATED_MF3 = MakeHandErrorFlag<HandFault>(34);
const HandFault HandFault::POSITION_SENSOR_UNCALIBRATED_RF2 = MakeHandErrorFlag<HandFault>(35);
const HandFault HandFault::POSITION_SENSOR_UNCALIBRATED_RF3 = MakeHandErrorFlag<HandFault>(36);
const HandFault HandFault::POSITION_SENSOR_UNCALIBRATED_LF2 = MakeHandErrorFlag<HandFault>(37);
const HandFault HandFault::POSITION_SENSOR_UNCALIBRATED_LF3 = MakeHandErrorFlag<HandFault>(38);
const HandFault HandFault::ZEROING_JOINT_STALL_THUMB2 = MakeHandErrorFlag<HandFault>(39);
const HandFault HandFault::ZEROING_JOINT_STALL_THUMB3 = MakeHandErrorFlag<HandFault>(40);
const HandFault HandFault::ZEROING_JOINT_STALL_THUMB4 = MakeHandErrorFlag<HandFault>(41);
const HandFault HandFault::ZEROING_JOINT_STALL_THUMB5 = MakeHandErrorFlag<HandFault>(42);
const HandFault HandFault::ZEROING_JOINT_STALL_FF2 = MakeHandErrorFlag<HandFault>(43);
const HandFault HandFault::ZEROING_JOINT_STALL_FF3 = MakeHandErrorFlag<HandFault>(44);
const HandFault HandFault::ZEROING_JOINT_STALL_FF4 = MakeHandErrorFlag<HandFault>(45);
const HandFault HandFault::ZEROING_JOINT_STALL_MF2 = MakeHandErrorFlag<HandFault>(46);
const HandFault HandFault::ZEROING_JOINT_STALL_MF3 = MakeHandErrorFlag<HandFault>(47);
const HandFault HandFault::ZEROING_JOINT_STALL_RF2 = MakeHandErrorFlag<HandFault>(48);
const HandFault HandFault::ZEROING_JOINT_STALL_RF3 = MakeHandErrorFlag<HandFault>(49);
const HandFault HandFault::ZEROING_JOINT_STALL_LF2 = MakeHandErrorFlag<HandFault>(50);
const HandFault HandFault::ZEROING_JOINT_STALL_LF3 = MakeHandErrorFlag<HandFault>(51);
const HandFault HandFault::ZEROING_BRAKE_TIMEOUT_THUMB2 = MakeHandErrorFlag<HandFault>(52);
const HandFault HandFault::ZEROING_BRAKE_TIMEOUT_THUMB3 = MakeHandErrorFlag<HandFault>(53);
const HandFault HandFault::ZEROING_BRAKE_TIMEOUT_THUMB4 = MakeHandErrorFlag<HandFault>(54);
const HandFault HandFault::ZEROING_BRAKE_TIMEOUT_THUMB5 = MakeHandErrorFlag<HandFault>(55);
const HandFault HandFault::ZEROING_BRAKE_TIMEOUT_FF2 = MakeHandErrorFlag<HandFault>(56);
const HandFault HandFault::ZEROING_BRAKE_TIMEOUT_FF3 = MakeHandErrorFlag<HandFault>(57);
const HandFault HandFault::ZEROING_BRAKE_TIMEOUT_FF4 = MakeHandErrorFlag<HandFault>(58);
const HandFault HandFault::ZEROING_BRAKE_TIMEOUT_MF2 = MakeHandErrorFlag<HandFault>(59);
const HandFault HandFault::ZEROING_BRAKE_TIMEOUT_MF3 = MakeHandErrorFlag<HandFault>(60);
const HandFault HandFault::ZEROING_BRAKE_TIMEOUT_RF2 = MakeHandErrorFlag<HandFault>(61);
const HandFault HandFault::ZEROING_BRAKE_TIMEOUT_RF3 = MakeHandErrorFlag<HandFault>(62);
const HandFault HandFault::ZEROING_BRAKE_TIMEOUT_LF2 = MakeHandErrorFlag<HandFault>(63);
const HandFault HandFault::ZEROING_BRAKE_TIMEOUT_LF3 = MakeHandErrorFlag<HandFault>(64);
const HandFault HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR0 = MakeHandErrorFlag<HandFault>(65);
const HandFault HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR1 = MakeHandErrorFlag<HandFault>(66);
const HandFault HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR2 = MakeHandErrorFlag<HandFault>(67);
const HandFault HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR3 = MakeHandErrorFlag<HandFault>(68);
const HandFault HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR4 = MakeHandErrorFlag<HandFault>(69);
const HandFault HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR5 = MakeHandErrorFlag<HandFault>(70);
const HandFault HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR6 = MakeHandErrorFlag<HandFault>(71);
const HandFault HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR7 = MakeHandErrorFlag<HandFault>(72);
const HandFault HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR8 = MakeHandErrorFlag<HandFault>(73);
const HandFault HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR9 = MakeHandErrorFlag<HandFault>(74);
const HandFault HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR10 = MakeHandErrorFlag<HandFault>(75);
const HandFault HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR11 = MakeHandErrorFlag<HandFault>(76);
const HandFault HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR12 = MakeHandErrorFlag<HandFault>(77);
HandFault operator|(const HandFault& lhs, const HandFault& rhs) {
  HandFault result;
  for (size_t i = 0; i < result.size(); ++i) result[i] = lhs[i] | rhs[i];
  return result;
}
HandFault operator&(const HandFault& lhs, const HandFault& rhs) {
  HandFault result;
  for (size_t i = 0; i < result.size(); ++i) result[i] = lhs[i] & rhs[i];
  return result;
}
const HandCriticalError HandCriticalError::NONE{};
const HandCriticalError HandCriticalError::UNKNOWN_ERROR = MakeHandErrorFlag<HandCriticalError>(0);
const HandCriticalError HandCriticalError::POSITION_SENSOR_VERSION_MISMATCH = MakeHandErrorFlag<HandCriticalError>(2);
const HandCriticalError HandCriticalError::TACTILE_SENSOR_VERSION_MISMATCH = MakeHandErrorFlag<HandCriticalError>(3);
const HandCriticalError HandCriticalError::MOTOR_DRIVER_VERSION_MISMATCH = MakeHandErrorFlag<HandCriticalError>(4);
const HandCriticalError HandCriticalError::POSITION_BOARD_DETECTION_ABNORMAL = MakeHandErrorFlag<HandCriticalError>(5);
const HandCriticalError HandCriticalError::TEMPERATURE_TOO_HIGH = MakeHandErrorFlag<HandCriticalError>(6);
const HandCriticalError HandCriticalError::TEMPERATURE_TOO_LOW = MakeHandErrorFlag<HandCriticalError>(7);
const HandCriticalError HandCriticalError::ZEROING_GLOBAL_TIMEOUT = MakeHandErrorFlag<HandCriticalError>(8);
HandCriticalError operator|(const HandCriticalError& lhs, const HandCriticalError& rhs) {
  HandCriticalError result;
  for (size_t i = 0; i < result.size(); ++i) result[i] = lhs[i] | rhs[i];
  return result;
}
HandCriticalError operator&(const HandCriticalError& lhs, const HandCriticalError& rhs) {
  HandCriticalError result;
  for (size_t i = 0; i < result.size(); ++i) result[i] = lhs[i] & rhs[i];
  return result;
}

// ProductType
const char* ToString(ProductType type) {
  switch (type) {
    case ProductType::GHand5:
      return "GHand5";
    case ProductType::AUTO:
      return "AUTO";
    case ProductType::GHandLite1:
      return "GHandLite1";
    default:
      return "UNKNOWN";
  }
}

// HandType
const char* ToString(HandType type) {
  switch (type) {
    case HandType::NONE:
      return "none";
    case HandType::LEFT_HAND:
      return "left_hand";
    case HandType::RIGHT_HAND:
      return "right_hand";
    default:
      return "UNKNOWN";
  }
}

// HandControlState
const char* ToString(HandControlState state) {
  switch (state) {
    case HandControlState::DISABLED: return "DISABLED";
    case HandControlState::ENABLED: return "ENABLED";
    default: return "UNKNOWN_STATE";
  }
}

const char* ToString(JointState state) {
  switch (state) {
    case JointState::DISABLED:
      return "DISABLED";
    case JointState::RUNNING:
      return "RUNNING";
    case JointState::STOPPED:
      return "STOPPED";
    case JointState::FAULT:
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
    case JointErrorCode::MOTOR_WINDING_OVERTEMPERATURE:
      return "MOTOR_WINDING_OVERTEMPERATURE";
    case static_cast<JointErrorCode>(14):
    case static_cast<JointErrorCode>(15):
    case static_cast<JointErrorCode>(16):
    case static_cast<JointErrorCode>(17):
    case static_cast<JointErrorCode>(18):
    case static_cast<JointErrorCode>(19):
    case static_cast<JointErrorCode>(20):
      return "RESERVED";
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
    case SdkError::PRODUCT_MISMATCH:
      return "PRODUCT_MISMATCH";
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
      return "MF_MCP";
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

}  // namespace ghand
