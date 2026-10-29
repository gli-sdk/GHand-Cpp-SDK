// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include "modbus_codec.h"

#include <chrono>
#include <thread>

namespace ghand {
namespace internal {

namespace {

constexpr float kAngleScale = 10.0f;

int16_t EncodeJointAngle(const JointCommand& joint) {
  return static_cast<int16_t>(joint.angle * kAngleScale);
}

}  // namespace

bool WaitCommandMailbox(const std::function<bool(uint16_t*)>& read,
                        int baseline, int timeout_ms) {
  auto start = std::chrono::steady_clock::now();
  bool busy_seen = false;
  while (std::chrono::steady_clock::now() - start <
         std::chrono::milliseconds(timeout_ms)) {
    uint16_t value = 0;
    if (read(&value)) {
      if (value == 0x0001) busy_seen = true;
      bool terminal = value == 0x0100 || value == 0x0200 || value == 0x0300;
      if (terminal && (busy_seen || value != baseline ||
          std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(300))) {
        return value == 0x0100;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return false;
}

bool WaitConnectionState(const std::function<bool(uint16_t*)>& read) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline) {
    uint16_t value = 0;
    if (read(&value)) {
      if ((value & 0xFF) == 1) return true;
      if ((value & 0xFF) == 3) return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return false;
}

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

std::string ParseFirmwareVersion(const uint8_t* raw_bytes, size_t len) {
  return ParseUtf8String(raw_bytes, len);
}

std::string ParsePackedFirmwareVersion(uint16_t raw) {
  uint8_t version_high = static_cast<uint8_t>((raw >> 8) & 0xFF);
  uint8_t version_low = static_cast<uint8_t>(raw & 0xFF);
  uint8_t major = (version_high >> 5) & 0x07;
  uint8_t minor = version_high & 0x1F;
  uint8_t patch = (version_low >> 4) & 0x0F;
  return "V" + std::to_string(major) + "." + std::to_string(minor) + "." +
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

HandData ParseHandInfo(const uint16_t* raw, size_t count) {
  HandData hs;
  if (count < 14) return hs;
  hs.joints_state = static_cast<HandControlState>(raw[0]);
  for (size_t i = 0; i < 6; ++i) {
    hs.error_warn[i] = static_cast<uint8_t>(raw[1 + i / 2] >> ((i % 2) * 8));
    hs.error_critical[i] = static_cast<uint8_t>(raw[10 + i / 2] >> ((i % 2) * 8));
  }
  for (size_t i = 0; i < 12; ++i) {
    hs.error_fault[i] = static_cast<uint8_t>(raw[4 + i / 2] >> ((i % 2) * 8));
  }
  hs.temperature = static_cast<int16_t>(raw[13]);
  return hs;
}

HandData ParseHandInfoLite1(const uint16_t* raw, size_t count) {
  HandData hs;
  if (count < 11) return hs;
  hs.joints_state = (raw[0] & 0xFF) == 0 ? HandControlState::DISABLED : HandControlState::ENABLED;
  for (size_t i = 0; i < 6; ++i) {
    hs.error_warn[i] = static_cast<uint8_t>(raw[1 + i / 2] >> ((i % 2) * 8));
    hs.error_fault[i] = static_cast<uint8_t>(raw[4 + i / 2] >> ((i % 2) * 8));
    hs.error_critical[i] = static_cast<uint8_t>(raw[7 + i / 2] >> ((i % 2) * 8));
  }
  hs.temperature = static_cast<int16_t>(raw[10]);
  return hs;
}

JointData ParseJointData(const uint16_t* raw, JointId joint_id) {
  JointData joint;
  joint.id = joint_id;

  uint8_t status_byte = raw[0] & 0xFF;
  uint8_t error_byte = (raw[0] >> 8) & 0xFF;
  joint.state = static_cast<JointState>(status_byte);
  joint.error = static_cast<JointErrorCode>(error_byte);
  joint.control_mode = static_cast<JointControlMode>(raw[1] & 0x0F);
  joint.loop_type = static_cast<JointLoopType>((raw[1] >> 4) & 0x0F);

  int16_t angle_raw = static_cast<int16_t>(raw[2]);
  joint.angle = angle_raw / 10.0f;

  int16_t speed = static_cast<int16_t>(raw[3]);
  int16_t current = static_cast<int16_t>(raw[4]);
  joint.velocity = speed;
  joint.current = current;

  return joint;
}

std::vector<JointData> ParseJoints(const uint16_t* raw, size_t count,
                               const std::vector<JointId>& valid_joints) {
  std::vector<JointData> joints;
  joints.reserve(valid_joints.size());

  for (JointId joint_id : valid_joints) {
    size_t offset = static_cast<size_t>(joint_id) * 5;
    if (offset + 4 >= count) continue;
    joints.push_back(ParseJointData(raw + offset, joint_id));
  }
  return joints;
}

std::vector<JointData> ParseJoints(const uint16_t* raw, size_t count,
                               const ProductConfig& config,
                               uint16_t start_addr) {
  std::vector<JointData> joints;
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
  return {static_cast<uint8_t>(first_reg & 0xFF),
          static_cast<uint8_t>(first_reg >> 8)};
}

std::vector<float> ParseTactileResultant(const uint16_t* raw, int region_index) {
  int base = 1 + region_index * 3;
  return {static_cast<int16_t>(raw[base]) / 10.0f,
          static_cast<int16_t>(raw[base + 1]) / 10.0f,
          static_cast<int16_t>(raw[base + 2]) / 10.0f};
}

std::vector<float> ParseTactileDistributed(const uint8_t* data_bytes,
                                           size_t byte_len, int count) {
  std::vector<float> forces;
  forces.reserve(count * 3);

  int expected_bytes = count * 3;
  size_t parse_len = (byte_len < static_cast<size_t>(expected_bytes))
                         ? byte_len
                         : static_cast<size_t>(expected_bytes);

  for (size_t i = 0; i + 2 < parse_len; i += 3) {
    int8_t fx_d = static_cast<int8_t>(data_bytes[i]);
    int8_t fy_d = static_cast<int8_t>(data_bytes[i + 1]);
    uint8_t fz_d = data_bytes[i + 2];
    forces.push_back(static_cast<float>(fx_d) / 10.0f);
    forces.push_back(static_cast<float>(fy_d) / 10.0f);
    forces.push_back(static_cast<float>(fz_d) / 10.0f);
  }
  return forces;
}

std::vector<uint16_t> EncodeJointCommand(const JointCommand& joint) {
  int16_t angle_raw = EncodeJointAngle(joint);
  const uint8_t joint_mode = static_cast<uint8_t>(joint.control_mode) |
      (static_cast<uint8_t>(joint.loop_type) << 4);
  const uint16_t mode_stop = (static_cast<uint16_t>(joint.control_state) << 8) | joint_mode;
  return {
      mode_stop,
      static_cast<uint16_t>(angle_raw),
      static_cast<uint16_t>(static_cast<int16_t>(joint.velocity)),
      static_cast<uint16_t>(static_cast<int16_t>(joint.current)),
      static_cast<uint16_t>(joint.kp),
      static_cast<uint16_t>(joint.kd),
  };
}

std::vector<uint16_t> EncodeHandCommand(
    const std::vector<JointCommand>& joints, HandControlMode mode,
    const ProductConfig& config, const std::map<JointId, HandControlMode>& modes) {
  std::map<JointId, JointCommand> commands;
  for (const auto& joint : joints) commands[joint.id] = joint;
  std::vector<uint16_t> registers(1, 0x0100 | static_cast<uint8_t>(mode));
  std::map<uint16_t, JointId> addresses;
  for (const auto& entry : config.joint_control_registers) addresses[entry.second] = entry.first;
  for (const auto& entry : addresses) {
    auto previous = modes.find(entry.second);
    HandControlMode default_mode = previous == modes.end() ? HandControlMode::POSITION : previous->second;
    JointCommand joint;
    joint.id = entry.second;
    joint.control_mode = static_cast<JointControlMode>(default_mode);
    joint.control_state = JointControlState::STOPPED;
    auto command = commands.find(entry.second);
    if (command != commands.end()) joint = command->second;
    auto encoded = EncodeJointCommand(joint);
    const size_t register_count = static_cast<size_t>(
        config.joint_control_register_count > 0
            ? config.joint_control_register_count
            : static_cast<int>(encoded.size()));
    if (encoded.size() > register_count) encoded.resize(register_count);
    size_t offset = entry.first - config.mode_register;
    registers.resize(offset);
    registers.insert(registers.end(), encoded.begin(), encoded.end());
  }
  return registers;
}

std::vector<uint8_t> EncodeEthercatCommand(
    const ProductConfig& config, HandControlMode mode, bool enabled,
    const std::map<JointId, JointCommand>& commands) {
  const bool lite1 = config.product_type == ProductType::GHandLite1;
  std::vector<uint8_t> bytes = {static_cast<uint8_t>(mode), static_cast<uint8_t>(enabled)};
  for (JointId id : config.valid_joints) {
    if (!config.joint_limits.count(id)) continue;
    JointCommand joint;
    joint.control_state = JointControlState::DISABLED;
    auto it = commands.find(id);
    if (it != commands.end()) joint = it->second;
    auto encoded = EncodeJointCommand(joint);
    bytes.push_back(static_cast<uint8_t>(encoded[0]));
    bytes.push_back(static_cast<uint8_t>(encoded[0] >> 8));
    if (lite1) {
      for (size_t i = 1; i <= 3; ++i) {
        bytes.push_back(static_cast<uint8_t>(encoded[i]));
        bytes.push_back(static_cast<uint8_t>(encoded[i] >> 8));
      }
    } else {
      for (size_t i = 1; i < encoded.size(); ++i) {
        bytes.push_back(static_cast<uint8_t>(encoded[i]));
        bytes.push_back(static_cast<uint8_t>(encoded[i] >> 8));
      }
    }
  }
  return bytes;
}

}  // namespace internal
}  // namespace ghand
