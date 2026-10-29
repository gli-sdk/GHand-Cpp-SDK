// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef INCLUDE_GHAND_TYPES_ERRORS_H_
#define INCLUDE_GHAND_TYPES_ERRORS_H_

#include <array>
#include <cstdint>

#include "ghand/export.h"

namespace ghand {

enum class JointErrorCode : uint8_t {
  NORMAL = 0,
  MOTOR_HARDWARE_OVERCURRENT = 1,
  MOTOR_CONTINUOUS_CURRENT_HIGH = 2,
  MOTOR_BUS_OVERCURRENT = 3,
  MOTOR_HARDWARE_OVERVOLTAGE = 4,
  MOTOR_HARDWARE_UNDERVOLTAGE = 5,
  MOTOR_PHASE_LOSS = 6,
  MOTOR_STALLED = 7,
  MOTOR_CHIP_OVERTEMPERATURE = 8,
  COMMUNICATION_LOST = 9,
  MOTOR_SOFTWARE_OVERVOLTAGE = 10,
  MOTOR_SOFTWARE_UNDERVOLTAGE = 11,
  MOTOR_OVERTEMPERATURE = 12,
  MOTOR_WINDING_OVERTEMPERATURE = 13,
};

GHAND_API const char* ToString(JointErrorCode error);  // NOLINT(runtime/string)

enum class SdkError : uint8_t {
  OK = 0,
  NOT_CONNECTED,
  INVALID_ARGUMENT,
  NOT_SUPPORTED,
  TIMEOUT,
  TRANSPORT_ERROR,
  PROTOCOL_ERROR,
  CONFIG_ERROR,
  DEVICE_REJECTED,
  INTERNAL_ERROR,
  PRODUCT_MISMATCH
};

GHAND_API const char* ToString(SdkError error);  // NOLINT(runtime/string)

// Firmware update error codes
enum class FirmwareUpdateError : int {
  SUCCESS = 0,
  PREPARE_COMMAND_FAILED,   // Write 0x5A pre-check failed
  ENTER_BOOT_MODE_FAILED,   // Enter BOOT state failed
  FOE_TRANSFER_FAILED,      // FOE transfer failed
  SWITCH_COMM_STATE_FAILED,  // Communication state switch failed
  NOT_RESETTABLE,            // Device reset is unavailable
  FIRMWARE_RESET_FAILED,     // Firmware reset command failed
  MAIN_CONTROLLER_FAILED,   // Main controller MCU upgrade failed
  POSITION_SENSOR_FAILED,   // Position sensor MCU upgrade failed
  TACTILE_SENSOR_FAILED,    // Tactile sensor MCU upgrade failed
  MOTOR_DRIVER_FAILED,      // Motor driver MCU upgrade failed
  RECONNECT_FAILED,  // Reconnect after update failed
  // Communication protocol does not support firmware update
  NOT_SUPPORTED,
  QUERY_FAILED,  // Failed to query upgrade results
};

// Protocol bytes in little-endian bit order; all reserved bits are preserved.
struct GHAND_API HandWarning : std::array<uint8_t, 6> {
  HandWarning() : std::array<uint8_t, 6>{} {}
  static const HandWarning NONE;
  static const HandWarning TEMPERATURE_SENSOR_1_ABNORMAL;
  static const HandWarning TEMPERATURE_SENSOR_2_ABNORMAL;
  static const HandWarning FAN_ABNORMAL;
  static const HandWarning TACTILE_BOARD_COMMUNICATION_ABNORMAL;
  static const HandWarning THUMB_TACTILE_SENSOR_DISCONNECTED;
  static const HandWarning FOREFINGER_TACTILE_SENSOR_DISCONNECTED;
  static const HandWarning MIDDLE_FINGER_TACTILE_SENSOR_DISCONNECTED;
  static const HandWarning RING_FINGER_TACTILE_SENSOR_DISCONNECTED;
  static const HandWarning LITTLE_FINGER_TACTILE_SENSOR_DISCONNECTED;
  static const HandWarning SELF_TEST_ERROR;
};

GHAND_API HandWarning operator|(const HandWarning& lhs, const HandWarning& rhs);
GHAND_API HandWarning operator&(const HandWarning& lhs, const HandWarning& rhs);

// Protocol bytes in little-endian bit order; all reserved bits are preserved.
struct GHAND_API HandFault : std::array<uint8_t, 12> {
  HandFault() : std::array<uint8_t, 12>{} {}
  static const HandFault NONE;
  static const HandFault JOINT_MOTOR_THUMB2;
  static const HandFault JOINT_MOTOR_THUMB3;
  static const HandFault JOINT_MOTOR_THUMB4;
  static const HandFault JOINT_MOTOR_THUMB5;
  static const HandFault JOINT_MOTOR_FF2;
  static const HandFault JOINT_MOTOR_FF3;
  static const HandFault JOINT_MOTOR_FF4;
  static const HandFault JOINT_MOTOR_MF2;
  static const HandFault JOINT_MOTOR_MF3;
  static const HandFault JOINT_MOTOR_RF2;
  static const HandFault JOINT_MOTOR_RF3;
  static const HandFault JOINT_MOTOR_LF2;
  static const HandFault JOINT_MOTOR_LF3;
  static const HandFault POSITION_SENSOR_ABNORMAL_THUMB2;
  static const HandFault POSITION_SENSOR_ABNORMAL_THUMB3;
  static const HandFault POSITION_SENSOR_ABNORMAL_THUMB4;
  static const HandFault POSITION_SENSOR_ABNORMAL_THUMB5;
  static const HandFault POSITION_SENSOR_ABNORMAL_FF2;
  static const HandFault POSITION_SENSOR_ABNORMAL_FF3;
  static const HandFault POSITION_SENSOR_ABNORMAL_FF4;
  static const HandFault POSITION_SENSOR_ABNORMAL_MF2;
  static const HandFault POSITION_SENSOR_ABNORMAL_MF3;
  static const HandFault POSITION_SENSOR_ABNORMAL_RF2;
  static const HandFault POSITION_SENSOR_ABNORMAL_RF3;
  static const HandFault POSITION_SENSOR_ABNORMAL_LF2;
  static const HandFault POSITION_SENSOR_ABNORMAL_LF3;
  static const HandFault POSITION_SENSOR_UNCALIBRATED_THUMB2;
  static const HandFault POSITION_SENSOR_UNCALIBRATED_THUMB3;
  static const HandFault POSITION_SENSOR_UNCALIBRATED_THUMB4;
  static const HandFault POSITION_SENSOR_UNCALIBRATED_THUMB5;
  static const HandFault POSITION_SENSOR_UNCALIBRATED_FF2;
  static const HandFault POSITION_SENSOR_UNCALIBRATED_FF3;
  static const HandFault POSITION_SENSOR_UNCALIBRATED_FF4;
  static const HandFault POSITION_SENSOR_UNCALIBRATED_MF2;
  static const HandFault POSITION_SENSOR_UNCALIBRATED_MF3;
  static const HandFault POSITION_SENSOR_UNCALIBRATED_RF2;
  static const HandFault POSITION_SENSOR_UNCALIBRATED_RF3;
  static const HandFault POSITION_SENSOR_UNCALIBRATED_LF2;
  static const HandFault POSITION_SENSOR_UNCALIBRATED_LF3;
  static const HandFault ZEROING_JOINT_STALL_THUMB2;
  static const HandFault ZEROING_JOINT_STALL_THUMB3;
  static const HandFault ZEROING_JOINT_STALL_THUMB4;
  static const HandFault ZEROING_JOINT_STALL_THUMB5;
  static const HandFault ZEROING_JOINT_STALL_FF2;
  static const HandFault ZEROING_JOINT_STALL_FF3;
  static const HandFault ZEROING_JOINT_STALL_FF4;
  static const HandFault ZEROING_JOINT_STALL_MF2;
  static const HandFault ZEROING_JOINT_STALL_MF3;
  static const HandFault ZEROING_JOINT_STALL_RF2;
  static const HandFault ZEROING_JOINT_STALL_RF3;
  static const HandFault ZEROING_JOINT_STALL_LF2;
  static const HandFault ZEROING_JOINT_STALL_LF3;
  static const HandFault ZEROING_BRAKE_TIMEOUT_THUMB2;
  static const HandFault ZEROING_BRAKE_TIMEOUT_THUMB3;
  static const HandFault ZEROING_BRAKE_TIMEOUT_THUMB4;
  static const HandFault ZEROING_BRAKE_TIMEOUT_THUMB5;
  static const HandFault ZEROING_BRAKE_TIMEOUT_FF2;
  static const HandFault ZEROING_BRAKE_TIMEOUT_FF3;
  static const HandFault ZEROING_BRAKE_TIMEOUT_FF4;
  static const HandFault ZEROING_BRAKE_TIMEOUT_MF2;
  static const HandFault ZEROING_BRAKE_TIMEOUT_MF3;
  static const HandFault ZEROING_BRAKE_TIMEOUT_RF2;
  static const HandFault ZEROING_BRAKE_TIMEOUT_RF3;
  static const HandFault ZEROING_BRAKE_TIMEOUT_LF2;
  static const HandFault ZEROING_BRAKE_TIMEOUT_LF3;
  static const HandFault ZEROING_MOTOR_COMMUNICATION_MOTOR0;
  static const HandFault ZEROING_MOTOR_COMMUNICATION_MOTOR1;
  static const HandFault ZEROING_MOTOR_COMMUNICATION_MOTOR2;
  static const HandFault ZEROING_MOTOR_COMMUNICATION_MOTOR3;
  static const HandFault ZEROING_MOTOR_COMMUNICATION_MOTOR4;
  static const HandFault ZEROING_MOTOR_COMMUNICATION_MOTOR5;
  static const HandFault ZEROING_MOTOR_COMMUNICATION_MOTOR6;
  static const HandFault ZEROING_MOTOR_COMMUNICATION_MOTOR7;
  static const HandFault ZEROING_MOTOR_COMMUNICATION_MOTOR8;
  static const HandFault ZEROING_MOTOR_COMMUNICATION_MOTOR9;
  static const HandFault ZEROING_MOTOR_COMMUNICATION_MOTOR10;
  static const HandFault ZEROING_MOTOR_COMMUNICATION_MOTOR11;
  static const HandFault ZEROING_MOTOR_COMMUNICATION_MOTOR12;
};

GHAND_API HandFault operator|(const HandFault& lhs, const HandFault& rhs);
GHAND_API HandFault operator&(const HandFault& lhs, const HandFault& rhs);

// Protocol bytes in little-endian bit order; all reserved bits are preserved.
struct GHAND_API HandCriticalError : std::array<uint8_t, 6> {
  HandCriticalError() : std::array<uint8_t, 6>{} {}
  static const HandCriticalError NONE;
  static const HandCriticalError UNKNOWN_ERROR;
  static const HandCriticalError POSITION_SENSOR_VERSION_MISMATCH;
  static const HandCriticalError TACTILE_SENSOR_VERSION_MISMATCH;
  static const HandCriticalError MOTOR_DRIVER_VERSION_MISMATCH;
  static const HandCriticalError POSITION_BOARD_DETECTION_ABNORMAL;
  static const HandCriticalError TEMPERATURE_TOO_HIGH;
  static const HandCriticalError TEMPERATURE_TOO_LOW;
  static const HandCriticalError ZEROING_GLOBAL_TIMEOUT;
};

GHAND_API HandCriticalError operator|(const HandCriticalError& lhs, const HandCriticalError& rhs);
GHAND_API HandCriticalError operator&(const HandCriticalError& lhs, const HandCriticalError& rhs);

}  // namespace ghand

#endif  // INCLUDE_GHAND_TYPES_ERRORS_H_
