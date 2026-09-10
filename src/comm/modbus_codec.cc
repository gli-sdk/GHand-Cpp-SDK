// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include "modbus_codec.h"

namespace ghand {
namespace internal {

namespace {

constexpr float kAngleScale = 10.0f;

float DecodeJointAngle(int16_t raw) {
  return raw / kAngleScale;
}

int16_t EncodeJointAngle(const JointCommand& joint) {
  return static_cast<int16_t>(joint.angle * kAngleScale);
}

}  // namespace

std::vector<uint8_t> RegistersToBytes(const std::vector<uint16_t>& registers) {
  std::vector<uint8_t> bytes;
  bytes.reserve(registers.size() * 2);
  for (uint16_t reg : registers) {
    bytes.push_back(static_cast<uint8_t>((reg >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(reg & 0xFF));
  }
  return bytes;
}

bool GetJointInputSpan(const ProductConfig& config, uint16_t* start_addr,
                       int* count) {
  if (start_addr == nullptr || count == nullptr ||
      config.valid_joints.empty() || config.joint_input_registers.empty()) {
    return false;
  }

  bool found = false;
  uint16_t start = 0xFFFF;
  uint16_t end = 0;
  for (JointId joint_id : config.valid_joints) {
    auto it = config.joint_input_registers.find(joint_id);
    if (it == config.joint_input_registers.end()) continue;
    if (it->second < start) start = it->second;
    uint16_t joint_end = static_cast<uint16_t>(it->second + 4);
    if (joint_end > end) end = joint_end;
    found = true;
  }

  if (!found) return false;
  *start_addr = start;
  *count = static_cast<int>(end - start + 1);
  return true;
}

static std::string ParseUtf8String(const uint8_t* raw_bytes, size_t len) {
  std::string str(reinterpret_cast<const char*>(raw_bytes), len);
  // Strip null terminators and padding
  size_t end = str.find('\0');
  if (end != std::string::npos) {
    str.resize(end);
  }
  return str;
}

std::string ParseDeviceName(const uint8_t* raw_bytes, size_t len) {
  return ParseUtf8String(raw_bytes, len);
}

std::string ParseHardwareVersion(const uint8_t* raw_bytes, size_t len) {
  return ParseUtf8String(raw_bytes, len);
}

std::string ParseFirmwareVersion(const uint8_t* raw_bytes, size_t len) {
  return ParseUtf8String(raw_bytes, len);
}

std::string ParsePackedFirmwareVersion(uint16_t raw) {
  uint8_t version_high = static_cast<uint8_t>((raw >> 8) & 0xFF);
  uint8_t version_low = static_cast<uint8_t>(raw & 0xFF);
  uint8_t major = (version_high >> 5) & 0x07;
  uint8_t minor = version_high & 0x1F;
  uint8_t patch = (version_low >> 4) & 0x0F;
  return std::to_string(major) + "." + std::to_string(minor) + "." +
         std::to_string(patch);
}

uint32_t ParseSerialNumber(const uint8_t* raw_bytes, size_t len) {
  uint32_t serial = 0;
  for (size_t i = 0; i < len && i < 4; ++i) {
    serial = (serial << 8) | raw_bytes[i];
  }
  return serial;
}

std::string ParseAsciiSerialNumber(const uint8_t* raw_bytes, size_t len) {
  return ParseUtf8String(raw_bytes, len > 19 ? 19 : len);
}

uint8_t ParseHandType(const uint8_t* raw_bytes, size_t len) {
  if (len < 2) return 0;
  return raw_bytes[1];
}

HandState ParseHandInfo(const uint16_t* raw, size_t count) {
  HandState hs;
  if (count >= 1) {
    hs.state = static_cast<State>(raw[0] & 0xFF);
  }
  const auto copy_low_high = [raw](size_t start, std::array<uint8_t, 6>* out) {
    for (size_t i = 0; i < out->size(); ++i) {
      const uint16_t reg = raw[start + i / 2];
      (*out)[i] = static_cast<uint8_t>((i % 2 == 0) ? reg : (reg >> 8));
    }
  };
  if (count >= 4) {
    copy_low_high(1, &hs.error_warn);
  }
  if (count >= 7) {
    copy_low_high(4, &hs.error_fault);
  }
  if (count >= 10) {
    copy_low_high(7, &hs.error_critical);
  }
  if (count >= 11) {
    hs.temperature = static_cast<int16_t>(raw[10]);
  }
  return hs;
}

Joint ParseJointData(const uint16_t* raw, JointId joint_id) {
  Joint joint;
  joint.id = joint_id;

  uint8_t status_byte = raw[0] & 0xFF;
  uint8_t error_byte = (raw[0] >> 8) & 0xFF;
  joint.state = static_cast<JointStateCode>(status_byte);
  joint.error = static_cast<JointErrorCode>(error_byte);
  joint.mode = static_cast<uint8_t>(raw[1] & 0xFF);
  joint.stop = static_cast<uint8_t>((raw[1] >> 8) & 0xFF);

  int16_t angle_raw = static_cast<int16_t>(raw[2]);
  joint.angle = angle_raw / 100.0f;

  int16_t speed = static_cast<int16_t>(raw[3]);
  int16_t current = static_cast<int16_t>(raw[4]);
  joint.velocity = speed;
  joint.current = current;

  return joint;
}

std::vector<Joint> ParseJoints(const uint16_t* raw, size_t count,
                               const std::vector<JointId>& valid_joints) {
  std::vector<Joint> joints;
  joints.reserve(valid_joints.size());

  for (JointId joint_id : valid_joints) {
    size_t offset = static_cast<size_t>(joint_id) * 5;
    if (offset + 4 >= count) continue;
    joints.push_back(ParseJointData(raw + offset, joint_id));
  }
  return joints;
}

std::vector<Joint> ParseJoints(const uint16_t* raw, size_t count,
                               const ProductConfig& config,
                               uint16_t start_addr) {
  std::vector<Joint> joints;
  joints.reserve(config.valid_joints.size());

  for (JointId joint_id : config.valid_joints) {
    auto it = config.joint_input_registers.find(joint_id);
    if (it == config.joint_input_registers.end()) continue;
    if (it->second < start_addr) continue;
    size_t offset = static_cast<size_t>(it->second - start_addr);
    if (offset + 4 >= count) continue;
    joints.push_back(ParseJointData(raw + offset, joint_id));
  }
  return joints;
}

std::pair<uint8_t, uint8_t> ParseTactileStateError(uint16_t first_reg) {
  return {static_cast<uint8_t>((first_reg >> 8) & 0xFF),
          static_cast<uint8_t>(first_reg & 0xFF)};
}

Force ParseTactileResultant(const uint16_t* raw, int region_index) {
  int base = 1 + region_index * 3;
  uint8_t raw_fx = (raw[base] >> 8) & 0xFF;
  uint8_t raw_fy = (raw[base + 1] >> 8) & 0xFF;
  uint8_t raw_fz = (raw[base + 2] >> 8) & 0xFF;

  float fx = (raw_fx >= 0x80) ? (raw_fx - 0x100) / 10.0f : raw_fx / 10.0f;
  float fy = (raw_fy >= 0x80) ? (raw_fy - 0x100) / 10.0f : raw_fy / 10.0f;
  float fz = raw_fz / 10.0f;

  Force f;
  f.x = fx;
  f.y = fy;
  f.z = fz;
  return f;
}

std::vector<Force> ParseTactileDistributed(const uint8_t* data_bytes,
                                           size_t byte_len, int count) {
  std::vector<Force> forces;
  forces.reserve(count);

  int expected_bytes = count * 3;
  size_t parse_len = (byte_len < static_cast<size_t>(expected_bytes))
                         ? byte_len
                         : static_cast<size_t>(expected_bytes);

  for (size_t i = 0; i + 2 < parse_len; i += 3) {
    int8_t fx_d = static_cast<int8_t>(data_bytes[i]);
    int8_t fy_d = static_cast<int8_t>(data_bytes[i + 1]);
    uint8_t fz_d = data_bytes[i + 2];
    Force f;
    f.x = fx_d / 10.0f;
    f.y = fy_d / 10.0f;
    f.z = fz_d / 10.0f;
    forces.push_back(f);
  }
  return forces;
}

std::vector<uint16_t> EncodeJointCommand(const JointCommand& joint,
                                         ControlMode mode) {
  int16_t angle_raw = EncodeJointAngle(joint);
  ControlMode joint_mode =
      joint.mode == ControlMode::POSITION ? mode : joint.mode;
  uint16_t mode_stop =
      static_cast<uint16_t>(((joint.stop & 0xFFu) << 8) |
                            (static_cast<uint8_t>(joint_mode) & 0x0Fu));
  return {
      mode_stop,
      static_cast<uint16_t>(angle_raw),
      static_cast<uint16_t>(static_cast<int16_t>(joint.velocity)),
      static_cast<uint16_t>(static_cast<int16_t>(joint.current)),
      static_cast<uint16_t>(joint.kp),
      static_cast<uint16_t>(joint.kd),
  };
}

}  // namespace internal
}  // namespace ghand
