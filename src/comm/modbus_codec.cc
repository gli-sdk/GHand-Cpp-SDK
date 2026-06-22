#include "modbus_codec.h"

namespace ghand {
namespace internal {

const std::map<JointId, uint16_t> kHoldingRegMap = {
    {JointId::THUMB_PIP, 0x0011},     {JointId::THUMB_MCP, 0x0013},
    {JointId::THUMB_SWING, 0x0015},   {JointId::THUMB_ROTATION, 0x0017},
    {JointId::FF_PIP, 0x0019},        {JointId::FF_MCP, 0x001B},
    {JointId::FF_SWING, 0x001D},      {JointId::MF_PIP, 0x001F},
    {JointId::MF_MCP, 0x0021},        {JointId::RF_PIP, 0x0023},
    {JointId::RF_MCP, 0x0025},        {JointId::LF_PIP, 0x0027},
    {JointId::LF_MCP, 0x0029},
};

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
    uint16_t joint_end = static_cast<uint16_t>(it->second + 2);
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

uint32_t ParseSerialNumber(const uint8_t* raw_bytes, size_t len) {
  uint32_t serial = 0;
  for (size_t i = 0; i < len && i < 4; ++i) {
    serial = (serial << 8) | raw_bytes[i];
  }
  return serial;
}

uint8_t ParseHandType(const uint8_t* raw_bytes, size_t len) {
  if (len < 2) return 0;
  return raw_bytes[1];
}

HandState ParseHandInfo(const uint16_t* raw, size_t count) {
  HandState hs;
  if (count >= 1) {
    hs.state = static_cast<State>((raw[0] >> 8) & 0xFF);
    hs.error = static_cast<ErrorCode>(raw[0] & 0xFF);
  }
  if (count >= 2) {
    hs.temperature = static_cast<int16_t>(raw[1]);
  }
  return hs;
}

Joint ParseJointData(const uint16_t* raw, JointId joint_id) {
  Joint joint;
  joint.id = joint_id;

  uint8_t status_byte = (raw[0] >> 8) & 0xFF;
  uint8_t error_byte = raw[0] & 0xFF;
  joint.state = static_cast<State>(status_byte);
  joint.error = static_cast<ErrorCode>(error_byte);

  int16_t angle_raw = static_cast<int16_t>(raw[1]);
  joint.angle = angle_raw / 10.0f;

  uint8_t speed = (raw[2] >> 8) & 0xFF;
  uint8_t torque = raw[2] & 0xFF;
  joint.velocity = static_cast<int8_t>(speed);
  joint.torque = static_cast<int8_t>(torque);

  return joint;
}

std::vector<Joint> ParseJoints(const uint16_t* raw, size_t count,
                               const std::vector<JointId>& valid_joints) {
  std::vector<Joint> joints;
  joints.reserve(valid_joints.size());

  for (JointId joint_id : valid_joints) {
    size_t offset = static_cast<size_t>(joint_id) * 3;
    if (offset + 2 >= count) continue;
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
    if (offset + 2 >= count) continue;
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

std::pair<uint16_t, uint16_t> EncodeJointCommand(const JointCommand& joint) {
  int16_t angle_raw = static_cast<int16_t>(joint.angle * 10);
  uint16_t reg0 = static_cast<uint16_t>(angle_raw);
  uint16_t reg1 = ((static_cast<uint16_t>(joint.velocity) & 0xFF) << 8) |
                  (static_cast<uint16_t>(joint.torque) & 0xFF);
  return {reg0, reg1};
}

}  // namespace internal
}  // namespace ghand
