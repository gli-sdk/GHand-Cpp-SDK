#ifndef SRC_INTERNAL_MODBUS_CODEC_H_
#define SRC_INTERNAL_MODBUS_CODEC_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ghand/types.h"
#include "product_config.h"

namespace ghand {
namespace internal {

// Mapping from controlled joint ID to holding-register base address.
extern const std::map<JointId, uint16_t> kHoldingRegMap;

// Convert a list of big-endian uint16 registers to bytes.
std::vector<uint8_t> RegistersToBytes(const std::vector<uint16_t>& registers);

// Parse device name from 16 bytes (8 registers).
std::string ParseDeviceName(const uint8_t* raw_bytes, size_t len = 16);

// Parse hardware version from 16 bytes (8 registers).
std::string ParseHardwareVersion(const uint8_t* raw_bytes, size_t len = 16);

// Parse firmware version from 16 bytes (8 registers).
std::string ParseFirmwareVersion(const uint8_t* raw_bytes, size_t len = 16);

// Parse serial number from 16 bytes (8 registers).
uint32_t ParseSerialNumber(const uint8_t* raw_bytes, size_t len = 16);

// Parse hand type from 2 bytes (1 register).
// Returns 0 for unknown, 1 for left hand, 2 for right hand.
uint8_t ParseHandType(const uint8_t* raw_bytes, size_t len = 2);

// Parse 2 input registers (0x1021~0x1022) into HandState.
HandState ParseHandInfo(const uint16_t* raw, size_t count = 2);

// Parse 3 input registers into Joint.
Joint ParseJointData(const uint16_t* raw, JointId joint_id);

// Parse joint data for all valid joints from a contiguous register block.
// The block is assumed to start at register 0x1023 and contain
// (max_joint_id + 1) * 3 registers.
std::vector<Joint> ParseJoints(const uint16_t* raw, size_t count,
                               const std::vector<JointId>& valid_joints);

// Parse tactile state and error from the first register (0x1080).
// Returns (state_byte, error_byte).
std::pair<uint8_t, uint8_t> ParseTactileStateError(uint16_t first_reg);

// Parse resultant force (Fx, Fy, Fz) for a tactile region.
// raw: first 16 registers starting at 0x1080.
// region_index: 0=THUMB, 1=FF, 2=MF, 3=RF, 4=LF.
Force ParseTactileResultant(const uint16_t* raw, int region_index);

// Parse distributed force data from raw bytes.
// count: Number of tactile points for this region.
std::vector<Force> ParseTactileDistributed(const uint8_t* data_bytes,
                                            size_t byte_len, int count);

// Encode a JointCommand into two holding-register values.
// Returns (reg0, reg1).
std::pair<uint16_t, uint16_t> EncodeJointCommand(const JointCommand& joint);

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_MODBUS_CODEC_H_
