// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef INCLUDE_GHAND_TYPES_ENUMS_H_
#define INCLUDE_GHAND_TYPES_ENUMS_H_

#include <cstdint>

#include "ghand/export.h"

namespace ghand {

// Hand type definitions
enum class HandType : uint8_t {
  NONE,
  LEFT_HAND,
  RIGHT_HAND,
};

/**
 * @brief Get string representation of hand type (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
GHAND_API const char* ToString(HandType type);  // NOLINT(runtime/string)

enum class HandControlState : uint16_t {
  DISABLED = 0,
  ENABLED = 1,
};

/**
 * @brief Get string representation of state (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
GHAND_API const char* ToString(HandControlState state);  // NOLINT(runtime/string)

enum class JointState : uint8_t {
  DISABLED = 0,
  RUNNING = 1,
  STOPPED = 2,
  FAULT = 3,
};

GHAND_API const char* ToString(JointState state);  // NOLINT(runtime/string)

// Communication type definitions
enum class CommType : uint8_t { ETHERCAT, CANFD, RS485 };

GHAND_API const char* ToString(CommType type);  // NOLINT(runtime/string)

/**
 * @brief Predefined RS485 baud rates.
 *
 * Each value maps directly to the baud-rate gear defined by the GHand device
 * protocol.
 */
enum class RS485BaudRate : uint8_t {
  RATE_57600 = 0x00,
  RATE_115200 = 0x01,
  RATE_230400 = 0x02,
  RATE_460800 = 0x03,
  RATE_921600 = 0x04,
  RATE_1000000 = 0x05,
};

/**
 * @brief Predefined CAN FD bit timing profiles.
 *
 * Naming format: TIMING_<arbitration bitrate>_<data bitrate>.
 * Sample points are fixed by the GHand device protocol for each profile.
 */
enum class CANFDBitTiming : uint8_t {
  // Arbitration: 500 Kbps @ 80%; data: 800 Kbps @ 80%.
  TIMING_500K_800K = 0x00,
  // Arbitration: 500 Kbps @ 80%; data: 1 Mbps @ 75%.
  TIMING_500K_1M = 0x01,
  // Arbitration: 500 Kbps @ 80%; data: 2 Mbps @ 80%.
  TIMING_500K_2M = 0x02,
  // Arbitration: 500 Kbps @ 80%; data: 4 Mbps @ 80%.
  TIMING_500K_4M = 0x03,
  // Arbitration: 1 Mbps @ 75%; data: 1 Mbps @ 75%.
  TIMING_1M_1M = 0x04,
  // Arbitration: 1 Mbps @ 75%; data: 2 Mbps @ 80%.
  TIMING_1M_2M = 0x05,
  // Arbitration: 1 Mbps @ 75%; data: 4 Mbps @ 80%.
  TIMING_1M_4M = 0x06,
  // Arbitration: 1 Mbps @ 75%; data: 5 Mbps @ 75%.
  TIMING_1M_5M = 0x07,
};

// Product type definitions
enum class ProductType : uint8_t {AUTO, GHand5,  GHandLite1 };

/**
 * @brief Get string representation of product type (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
GHAND_API const char* ToString(ProductType type);  // NOLINT(runtime/string)

enum class JointId : uint8_t {
  THUMB_IP,
  THUMB_MCP,
  THUMB_TMC_FE,
  THUMB_TMC_AA,
  THUMB_TMC_PS,
  FF_DIP,
  FF_PIP,
  FF_MCP,
  FF_MCP_AA,
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
 * @brief Get string representation of joint ID (for debug/logging)
 * @warning Do not call in real-time control loops (string processing overhead)
 */
GHAND_API const char* ToString(JointId id);  // NOLINT(runtime/string)

// Control mode definitions
enum class HandControlMode : uint8_t {
  POSITION = 0,
  VELOCITY = 1,
  CURRENT = 2,
  MIT = 3,
  CUSTOM = 0x0F,
};

enum class HandRuntimeState : uint8_t {
  INITIALIZED = 0, POWER_ON_SELF_TEST = 1, IDLE = 2, MOVING = 3,
  FAULT = 4, LOW_POWER = 5, FIRMWARE_UPGRADE = 6
};

enum class JointControlMode : uint8_t {
  POSITION = 0, VELOCITY = 1, CURRENT = 2, MIT = 3
};

enum class JointLoopType : uint8_t { SEMI_CLOSED_LOOP = 0, CLOSED_LOOP = 1 };
enum class TactileSensorId : uint8_t { THUMB = 0, FF = 1, MF = 2, RF = 3, LF = 4 };
enum class GestureType : uint8_t { OPEN_HAND, FIST, OK, THUMBS_UP, SIX_SIGN };

enum class JointControlState : uint8_t { DISABLED = 0, RUNNING = 1, STOPPED = 2 };

}  // namespace ghand

#endif  // INCLUDE_GHAND_TYPES_ENUMS_H_
