// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRC_COMM_MODBUS_CODEC_H_
#define SRC_COMM_MODBUS_CODEC_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ghand/types.h"
#include "product_config.h"

namespace ghand {
namespace internal {

// Convert a list of big-endian uint16 registers to bytes.
std::vector<uint8_t> RegistersToBytes(const std::vector<uint16_t>& registers);

// Get the smallest contiguous input-register span covering valid joints.
bool GetJointInputSpan(const ProductConfig& config, uint16_t* start_addr,
                       int* count);

// Parse device name from 16 bytes (8 registers).
std::string ParseDeviceName(const uint8_t* raw_bytes, size_t len = 16);

// Parse hardware version from 16 bytes (8 registers).
std::string ParseHardwareVersion(const uint8_t* raw_bytes, size_t len = 16);

// Parse firmware version from 16 bytes (8 registers).
std::string ParseFirmwareVersion(const uint8_t* raw_bytes, size_t len = 16);

// Parse packed firmware version from one uint16 register.
std::string ParsePackedFirmwareVersion(uint16_t raw);

// Parse serial number from 16 bytes (8 registers).
uint32_t ParseSerialNumber(const uint8_t* raw_bytes, size_t len = 16);

// Parse product serial number from 19 ASCII bytes.
std::string ParseAsciiSerialNumber(const uint8_t* raw_bytes, size_t len = 19);

// Parse hand type from 2 bytes (1 register).
// Returns 0 for unknown, 1 for left hand, 2 for right hand.
uint8_t ParseHandType(const uint8_t* raw_bytes, size_t len = 2);

// Parse 11 input registers (0x1047~0x1051) into HandState.
HandState ParseHandInfo(const uint16_t* raw, size_t count = 11);

// Parse 5 input registers into Joint.
Joint ParseJointData(const uint16_t* raw, JointId joint_id);

// Parse joint data for all valid joints from a contiguous register block.
// The block is assumed to start at register 0x1023 and contain
// (max_joint_id + 1) * 5 registers for GHand5.
std::vector<Joint> ParseJoints(const uint16_t* raw, size_t count,
                               const std::vector<JointId>& valid_joints);

std::vector<Joint> ParseJoints(const uint16_t* raw, size_t count,
                               const ProductConfig& config,
                               uint16_t start_addr);

// Parse tactile state and error from the first register (0x1110).
// Returns (state_byte, error_byte).
std::pair<uint8_t, uint8_t> ParseTactileStateError(uint16_t first_reg);

// Parse resultant force (Fx, Fy, Fz) for a tactile region.
// raw: first 16 registers starting at 0x1110.
// region_index: 0=THUMB, 1=FF, 2=MF, 3=RF, 4=LF.
Force ParseTactileResultant(const uint16_t* raw, int region_index);

// Parse distributed force data from raw bytes.
// count: Number of tactile points for this region.
std::vector<Force> ParseTactileDistributed(const uint8_t* data_bytes,
                                            size_t byte_len, int count);

// Encode a JointCommand into six holding-register values.
std::vector<uint16_t> EncodeJointCommand(const JointCommand& joint,
                                         ControlMode mode);

}  // namespace internal
}  // namespace ghand

#endif  // SRC_COMM_MODBUS_CODEC_H_
