#include "canfd_comm.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

#include "ghand/logging.h"
#include "logging_macros.h"
#include "modbus_codec.h"

namespace ghand {
namespace internal {

// CANFD valid data lengths (DLC mapping)
static const uint8_t kCanfdLengths[] = {0,  1,  2,  3,  4,  5,  6,  7,
                                         8,  12, 16, 20, 24, 32, 48, 64};

static uint8_t NearestCanfdLength(uint8_t len) {
  for (uint8_t vl : kCanfdLengths) {
    if (vl >= len) return vl;
  }
  return 64;
}

static bool IsIndexFingerControlJoint(JointId id) {
  return id == JointId::FF_PIP || id == JointId::FF_MCP ||
         id == JointId::FF_MCP_AA;
}

static void AppendEncodedJoint(const JointCommand& joint,
                               std::vector<uint16_t>* registers) {
  auto regs = EncodeJointCommand(joint);
  registers->push_back(regs.first);
  registers->push_back(regs.second);
}

static std::vector<uint8_t> EncodeJointBlock(
    const std::vector<JointCommand>& joints) {
  std::vector<uint16_t> registers;
  registers.reserve(joints.size() * 2);
  for (const auto& joint : joints) {
    AppendEncodedJoint(joint, &registers);
  }
  return RegistersToBytes(registers);
}

static std::vector<uint8_t> BuildCanfdRegisterPayload(
    int addr, int count, const std::vector<uint16_t>& values) {
  std::vector<uint16_t> registers;
  registers.reserve(2 + static_cast<size_t>(count));
  registers.push_back(static_cast<uint16_t>(addr));
  registers.push_back(static_cast<uint16_t>(count));
  for (int i = 0; i < count; ++i) {
    uint16_t value = i < static_cast<int>(values.size()) ? values[i] : 0;
    registers.push_back(value);
  }
  return RegistersToBytes(registers);
}

static void AddUniquePayload(std::vector<std::vector<uint8_t>>* values,
                             const std::vector<uint8_t>& payload) {
  if (std::find(values->begin(), values->end(), payload) == values->end()) {
    values->push_back(payload);
  }
}

static std::vector<std::vector<uint8_t>> BuildConnectionPayloads(
    const ProductConfig& config, bool delete_connection) {
  std::vector<std::vector<uint8_t>> payloads;
  if (delete_connection) {
    AddUniquePayload(&payloads,
                     BuildCanfdRegisterPayload(
                         config.canfd_connection_delete_register,
                         config.canfd_connection_delete_count,
                         config.canfd_connection_delete_values));
  } else {
    AddUniquePayload(&payloads,
                     BuildCanfdRegisterPayload(
                         config.canfd_connection_timer_register,
                         config.canfd_connection_timer_count,
                         config.canfd_connection_timer_values));
  }

  if (config.name.empty()) {
    AddUniquePayload(&payloads,
                     delete_connection
                         ? BuildCanfdRegisterPayload(
                               0x0036, 3, {0x0000, 0x0000, 0x0000})
                         : BuildCanfdRegisterPayload(0x0037, 2,
                                                      {0x0000, 0x0000}));
  }
  return payloads;
}

CANFDComm::CANFDComm(const ProductConfig& config)
    : driver_(CreateCANFDDriver()), config_(config) {}

CANFDComm::~CANFDComm() { Disconnect(); }

// Arbitration helpers (Python SDK compatible)

uint32_t CANFDComm::PackArbitration(int src_id, int dst_id, int ack,
                                     int func_code, int start, int end,
                                     int toggle, int seg_num) {
  return ((src_id & 0x3F) << 23) | ((dst_id & 0x3F) << 17) |
         ((ack & 0x01) << 16) | ((func_code & 0xFF) << 8) |
         ((start & 0x01) << 7) | ((end & 0x01) << 6) |
         ((toggle & 0x01) << 5) | (seg_num & 0x1F);
}

void CANFDComm::UnpackArbitration(uint32_t can_id, int* src_id, int* dst_id,
                                   int* ack, int* func_code, int* start,
                                   int* end, int* toggle, int* seg_num) {
  if (src_id) *src_id = (can_id >> 23) & 0x3F;
  if (dst_id) *dst_id = (can_id >> 17) & 0x3F;
  if (ack) *ack = (can_id >> 16) & 0x01;
  if (func_code) *func_code = (can_id >> 8) & 0xFF;
  if (start) *start = (can_id >> 7) & 0x01;
  if (end) *end = (can_id >> 6) & 0x01;
  if (toggle) *toggle = (can_id >> 5) & 0x01;
  if (seg_num) *seg_num = can_id & 0x1F;
}

// Frame I/O

bool CANFDComm::SendFrame(uint32_t can_id, const uint8_t* data, uint8_t len) {
  if (!driver_) return false;

  canfd::Frame frame;
  frame.id = can_id;
  memcpy(frame.data, data, len);
  frame.len = NearestCanfdLength(len);
  frame.is_fd = true;
  frame.is_extended = true;

  return driver_->Send(frame) == 0;
}

bool CANFDComm::RecvFrame(uint32_t* can_id, uint8_t* data, uint8_t* len,
                          int timeout_ms) {
  if (!driver_) return false;

  canfd::Frame frame;
  if (driver_->Receive(&frame, timeout_ms) != 0) return false;

  *can_id = frame.id;
  uint8_t copy_len = frame.len;
  if (copy_len > 64) copy_len = 64;
  memcpy(data, frame.data, copy_len);
  *len = copy_len;
  return true;
}

// Modbus-over-CANFD transport

bool CANFDComm::ReadRegisters(int addr, int count,
                              std::vector<uint8_t>* out_bytes,
                              int func_code, int timeout_ms) {
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  // Command frame data: AddrHi, AddrLo, CountHi, CountLo
  uint8_t cmd_data[4];
  cmd_data[0] = (addr >> 8) & 0xFF;
  cmd_data[1] = addr & 0xFF;
  cmd_data[2] = (count >> 8) & 0xFF;
  cmd_data[3] = count & 0xFF;

  uint32_t can_id =
      PackArbitration(src_id_, dst_id_, 0, func_code, 1, 1, 0, 0);
  if (!SendFrame(can_id, cmd_data, 4)) return false;

  // Gather response segments
  std::map<int, std::vector<uint8_t>> segments;
  auto start_time = std::chrono::steady_clock::now();

  while (true) {
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    int elapsed_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
            .count());
    if (elapsed_ms >= timeout_ms) return false;

    uint32_t resp_id = 0;
    uint8_t resp_data[64];
    uint8_t resp_len = 0;
    int remaining = timeout_ms - elapsed_ms;
    if (!RecvFrame(&resp_id, resp_data, &resp_len, std::max(remaining, 10)))
      continue;

    int r_src = 0, r_dst = 0, r_ack = 0, r_fc = 0, r_start = 0, r_end = 0,
        r_toggle = 0, r_seg = 0;
    UnpackArbitration(resp_id, &r_src, &r_dst, &r_ack, &r_fc, &r_start,
                       &r_end, &r_toggle, &r_seg);

    if (r_ack != 1 || r_dst != src_id_ || r_src != dst_id_) continue;

    // Exception response?
    if (r_fc == (0x80 | func_code)) return false;
    if (r_fc != func_code) continue;

    // Single-frame response
    if (r_start == 1 && r_end == 1) {
      if (resp_len > 1) {
        out_bytes->assign(resp_data + 1, resp_data + resp_len);
      }
      return true;
    }

    // Multi-frame response
    std::vector<uint8_t> seg_data(resp_data, resp_data + resp_len);
    if (r_seg == 0 && seg_data.size() > 1) {
      seg_data.erase(seg_data.begin());  // strip Modbus byte-count prefix
    }
    segments[r_seg] = std::move(seg_data);

    if (r_end == 1) {
      // Reassemble
      out_bytes->clear();
      for (const auto& kv : segments) {
        out_bytes->insert(out_bytes->end(), kv.second.begin(),
                          kv.second.end());
      }
      return true;
    }
  }
}

bool CANFDComm::WriteRegisters(int addr, const std::vector<uint8_t>& data,
                               int timeout_ms) {
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  uint8_t func_code;
  std::vector<uint8_t> payload;

  if (data.size() == 2) {
    // Single register write (FC 0x06)
    func_code = 0x06;
    uint16_t value = (data[0] << 8) | data[1];
    payload.resize(4);
    payload[0] = (addr >> 8) & 0xFF;
    payload[1] = addr & 0xFF;
    payload[2] = (value >> 8) & 0xFF;
    payload[3] = value & 0xFF;
  } else {
    // Multiple register write (FC 0x10)
    func_code = 0x10;
    int reg_count = static_cast<int>(data.size() / 2);
    payload.resize(5 + data.size());
    payload[0] = (addr >> 8) & 0xFF;
    payload[1] = addr & 0xFF;
    payload[2] = (reg_count >> 8) & 0xFF;
    payload[3] = reg_count & 0xFF;
    payload[4] = static_cast<uint8_t>(data.size());
    memcpy(payload.data() + 5, data.data(), data.size());
  }

  // Segmented transmit
  const int kSegSize = 64;
  int num_segs = (static_cast<int>(payload.size()) + kSegSize - 1) / kSegSize;
  for (int seg_idx = 0; seg_idx < num_segs; ++seg_idx) {
    int start = seg_idx == 0 ? 1 : 0;
    int end = seg_idx == num_segs - 1 ? 1 : 0;
    int toggle = seg_idx % 2;
    int offset = seg_idx * kSegSize;
    int seg_len =
        std::min(kSegSize, static_cast<int>(payload.size()) - offset);

    uint32_t can_id = PackArbitration(src_id_, dst_id_, 0, func_code, start,
                                       end, toggle, seg_idx);
    if (!SendFrame(can_id, payload.data() + offset, seg_len)) return false;
  }

  // Wait for single-frame response
  auto start_time = std::chrono::steady_clock::now();
  while (true) {
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    int elapsed_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
            .count());
    if (elapsed_ms >= timeout_ms) return false;

    uint32_t resp_id = 0;
    uint8_t resp_data[64];
    uint8_t resp_len = 0;
    int remaining = timeout_ms - elapsed_ms;
    if (!RecvFrame(&resp_id, resp_data, &resp_len, std::max(remaining, 10)))
      continue;

    int r_src = 0, r_dst = 0, r_ack = 0, r_fc = 0, r_start = 0, r_end = 0;
    UnpackArbitration(resp_id, &r_src, &r_dst, &r_ack, &r_fc, &r_start,
                       &r_end, nullptr, nullptr);

    if (r_ack != 1 || r_dst != src_id_ || r_src != dst_id_) continue;
    if (r_fc == (0x80 | func_code)) return false;
    if (r_fc == func_code && r_start == 1 && r_end == 1) return true;
  }
}

bool CANFDComm::WriteSingleRegister(int addr, uint16_t value, int timeout_ms) {
  std::vector<uint8_t> data = {static_cast<uint8_t>((value >> 8) & 0xFF),
                                static_cast<uint8_t>(value & 0xFF)};
  return WriteRegisters(addr, data, timeout_ms);
}

// Connection management

bool CANFDComm::NodeIdDetection(int timeout_ms) {
  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    uint32_t resp_id = 0;
    uint8_t resp_data[64];
    uint8_t resp_len = 0;
    if (!RecvFrame(&resp_id, resp_data, &resp_len, 50)) continue;

    int r_src = 0, r_dst = 0, r_ack = 0, r_fc = 0;
    UnpackArbitration(resp_id, &r_src, &r_dst, &r_ack, &r_fc, nullptr,
                       nullptr, nullptr, nullptr);

    if (r_ack == 1 && r_fc == 0x09 && resp_len >= 1) {
      if (resp_data[0] == dst_id_) {
        GHAND_LOG_INFO("Detected slave Node ID 0x"
                       << std::hex << static_cast<int>(dst_id_));
        return true;
      }
    }
  }
  return false;
}

bool CANFDComm::EstablishConnection(int timeout_ms) {
  const uint8_t dst_ids[] = {0x31, 0x32};

  auto payloads = BuildConnectionPayloads(config_, false);
  for (const auto& data : payloads) {
    for (uint8_t dst : dst_ids) {
      uint32_t can_id =
          PackArbitration(src_id_, dst, 0, 0x02, 1, 1, 0, 0);
      SendFrame(can_id, data.data(), static_cast<uint8_t>(data.size()));

      auto deadline = std::chrono::steady_clock::now() +
                       std::chrono::milliseconds(timeout_ms);
      while (std::chrono::steady_clock::now() < deadline) {
        uint32_t resp_id = 0;
        uint8_t resp_data[64];
        uint8_t resp_len = 0;
        if (!RecvFrame(&resp_id, resp_data, &resp_len, 50)) continue;

        int r_src = 0, r_dst = 0, r_ack = 0, r_fc = 0;
        UnpackArbitration(resp_id, &r_src, &r_dst, &r_ack, &r_fc, nullptr,
                           nullptr, nullptr, nullptr);

        if (r_ack != 1 || r_dst != src_id_ || r_src != dst) continue;
        if (r_fc == 0x82) {
          if (resp_len > 0 && resp_data[0] == 0x03) {
            uint8_t previous_dst = dst_id_;
            dst_id_ = dst;
            std::vector<uint8_t> bytes;
            if (ReadRegisters(0x1000, 1, &bytes, 0x04, 500)) {
              return true;
            }
            dst_id_ = previous_dst;
          }
          break;  // Exception, try next dst/payload
        }
        if (r_fc == 0x02) {
          dst_id_ = dst;
          return true;
        }
      }
    }
  }
  return false;
}

void CANFDComm::DeleteConnection() {
  auto payloads = BuildConnectionPayloads(config_, true);
  for (const auto& data : payloads) {
    uint32_t can_id = PackArbitration(src_id_, dst_id_, 0, 0x05, 1, 1, 0, 0);
    SendFrame(can_id, data.data(), static_cast<uint8_t>(data.size()));
  }
}

int CANFDComm::Connect(const std::string& device_name) {
  GHAND_LOG_INFO("CANFD connecting to: " << device_name);

  if (!driver_) {
    GHAND_LOG_ERROR("CANFD driver not available");
    return -1;
  }

  if (driver_->Open(device_name, 1000000, 5000000) != 0) {
    GHAND_LOG_ERROR("Failed to open CANFD channel: " << device_name);
    return -2;
  }

  const uint8_t dst_ids[] = {0x31, 0x32};
  auto payloads = BuildConnectionPayloads(config_, true);
  for (uint8_t dst : dst_ids) {
    for (const auto& data : payloads) {
      uint32_t can_id = PackArbitration(src_id_, dst, 0, 0x05, 1, 1, 0, 0);
      SendFrame(can_id, data.data(), static_cast<uint8_t>(data.size()));
    }
  }

  connected_.store(true);

  if (!EstablishConnection(500)) {
    GHAND_LOG_ERROR("Failed to establish CANFD connection");
    driver_->Close();
    connected_.store(false);
    return -3;
  }

  bool should_start_poll = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    joints_poll_enabled_ = static_cast<bool>(joints_cb_);
    hand_state_poll_enabled_ = static_cast<bool>(hand_state_cb_);
    should_start_poll = joints_poll_enabled_ || hand_state_poll_enabled_ ||
                        tactile_poll_enabled_;
  }
  if (should_start_poll) {
    EnsurePollStarted();
  }

  GHAND_LOG_INFO("CANFD device connected (" << device_name << ")");
  return 0;
}

int CANFDComm::Disconnect() {
  GHAND_LOG_INFO("CANFD disconnecting");

  StopPoll();
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    joints_poll_enabled_ = false;
    hand_state_poll_enabled_ = false;
    tactile_poll_enabled_ = false;
  }

  if (connected_.load()) {
    DeleteConnection();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  connected_.store(false);
  if (driver_) {
    driver_->Close();
  }

  GHAND_LOG_INFO("CANFD disconnected");
  return 0;
}

std::map<std::string, std::string> CANFDComm::SearchAdapters() {
  if (driver_) {
    return driver_->EnumerateAdapters();
  }
  return {};
}

bool CANFDComm::SetSlaveId(uint8_t slave_id) {
  if (!IsConnected()) return false;
  if (!WriteSingleRegister(0x0000, slave_id)) return false;
  dst_id_ = slave_id;
  GHAND_LOG_INFO("CANFD slave ID set to 0x"
                 << std::hex << static_cast<int>(slave_id) << std::dec);
  return true;
}

// Device info

bool CANFDComm::ReadInputBytes(int addr, int count,
                               std::vector<uint8_t>* bytes) {
  if (bytes == nullptr) return false;
  return ReadRegisters(addr, count, bytes, 0x04);
}

DeviceInfo CANFDComm::GetDeviceInfo() {
  DeviceInfo info;
  std::vector<uint8_t> bytes;
  auto read_version = [this](int addr, const char* name) -> std::string {
    std::vector<uint8_t> version_bytes;
    if (!ReadInputBytes(addr, 1, &version_bytes) ||
        version_bytes.size() < 2) {
      GHAND_LOG_WARNING("Failed to read " << name << " version over CANFD");
      return "";
    }
    uint16_t raw =
        (static_cast<uint16_t>(version_bytes[0]) << 8) | version_bytes[1];
    return ParsePackedFirmwareVersion(raw);
  };

  if (!ReadInputBytes(0x1000, 8, &bytes)) {
    GHAND_LOG_ERROR("Failed to read device name over CANFD");
    return info;
  }
  info.device_name = ParseDeviceName(bytes.data(), bytes.size());

  if (!ReadInputBytes(0x1008, 8, &bytes)) {
    GHAND_LOG_ERROR("Failed to read hardware version over CANFD");
    return info;
  }
  info.hardware_version = ParseHardwareVersion(bytes.data(), bytes.size());

  if (!ReadInputBytes(0x1010, 8, &bytes)) {
    GHAND_LOG_ERROR("Failed to read software version over CANFD");
    return info;
  }
  info.software_version = ParseFirmwareVersion(bytes.data(), bytes.size());

  if (!ReadInputBytes(0x1018, 8, &bytes)) {
    GHAND_LOG_ERROR("Failed to read serial number over CANFD");
    return info;
  }
  info.serial_number = ParseSerialNumber(bytes.data(), bytes.size());

  info.firmware_package_version = read_version(0x1185, "firmware package");
  info.position_sensor_version = read_version(0x1186, "position sensor");
  info.tactile_sensor_version = read_version(0x1187, "tactile MCU");
  info.motor_driver_version = read_version(0x1188, "motor driver");
  info.thumb_tactile_sensor_version =
      read_version(0x1189, "thumb tactile sensor");
  info.finger_tactile_sensor_version =
      read_version(0x118A, "finger tactile sensor");
  return info;
}

HandType CANFDComm::GetHandType() {
  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(0x1020, 1, &bytes)) {
    GHAND_LOG_ERROR("Failed to read hand type over CANFD");
    return HandType::NONE;
  }
  uint8_t type = ParseHandType(bytes.data(), bytes.size());
  if (type == 1) return HandType::LEFT;
  if (type == 2) return HandType::RIGHT;
  return HandType::NONE;
}

// Motion control

bool CANFDComm::MoveJoints(const std::vector<JointCommand>& joints,
                           ControlMode mode) {
  if (!IsConnected() || joints.empty()) return false;

  if (config_.mode_register >= 0) {
    uint16_t mode_value = (static_cast<uint16_t>(mode) << 8) & 0xFF00;
    if (!WriteSingleRegister(config_.mode_register, mode_value)) return false;
  }

  if (config_.per_joint_mode_control) {
    const auto& control_map = config_.joint_control_registers.empty()
                                  ? kHoldingRegMap
                                  : config_.joint_control_registers;
    for (const auto& joint : joints) {
      auto it = control_map.find(joint.id);
      if (it == control_map.end()) continue;

      std::vector<uint16_t> registers;
      registers.push_back((static_cast<uint16_t>(mode) << 8) & 0xFF00);
      auto regs = EncodeJointCommand(joint);
      registers.push_back(regs.first);
      registers.push_back(regs.second);
      if (!WriteRegisters(it->second, RegistersToBytes(registers))) {
        return false;
      }
    }
    return true;
  }

  const JointCommand* ff_swing = nullptr;
  for (const auto& joint : joints) {
    command_cache_[joint.id] = joint;
    if (joint.id == JointId::FF_MCP_AA) {
      ff_swing = &joint;
    }
  }

  if (ff_swing != nullptr) {
    auto get_cached_or_default =
        [this, ff_swing](JointId id) -> JointCommand {
      auto it = command_cache_.find(id);
      if (it != command_cache_.end()) return it->second;
      JointCommand joint;
      joint.id = id;
      joint.angle = 0.0f;
      joint.velocity = ff_swing->velocity;
      joint.torque = ff_swing->torque;
      return joint;
    };

    std::vector<JointCommand> index_group;
    index_group.push_back(get_cached_or_default(JointId::FF_PIP));
    index_group.push_back(get_cached_or_default(JointId::FF_MCP));
    index_group.push_back(get_cached_or_default(JointId::FF_MCP_AA));

    auto it = kHoldingRegMap.find(JointId::FF_PIP);
    if (it == kHoldingRegMap.end()) return false;
    if (!WriteRegisters(it->second, EncodeJointBlock(index_group))) {
      return false;
    }
  }

  // Write each joint
  for (const auto& joint : joints) {
    if (ff_swing != nullptr && IsIndexFingerControlJoint(joint.id)) continue;

    auto it = kHoldingRegMap.find(joint.id);
    if (it == kHoldingRegMap.end()) continue;

    std::vector<uint16_t> registers;
    auto regs = EncodeJointCommand(joint);
    registers.push_back(regs.first);
    registers.push_back(regs.second);
    if (!WriteRegisters(it->second, RegistersToBytes(registers))) return false;
  }

  return true;
}

void CANFDComm::Stop() {
  if (!IsConnected()) return;
  if (config_.stop_register >= 0) {
    WriteSingleRegister(config_.stop_register, 0x0001);
    return;
  }

  const auto& control_map = config_.joint_control_registers.empty()
                                ? kHoldingRegMap
                                : config_.joint_control_registers;
  for (JointId joint_id : config_.valid_joints) {
    auto it = control_map.find(joint_id);
    if (it != control_map.end()) {
      WriteSingleRegister(it->second, 0x0001);
    }
  }
}

// System operations

bool CANFDComm::ClearFault() {
  if (!IsConnected()) return false;
  if (!WriteSingleRegister(0x0001, 0x0100)) return false;
  GHAND_LOG_INFO("Fault cleared");
  return true;
}

bool CANFDComm::InitJoint() {
  if (!IsConnected()) return false;
  if (!WriteSingleRegister(0x0002, 0x0001)) return false;
  GHAND_LOG_INFO("Joint initialization completed");
  return true;
}

// Tactile sensor

bool CANFDComm::WriteTactileControl(uint16_t command) {
  if (!IsConnected()) return false;
  return WriteSingleRegister(config_.tactile_control_register, command);
}

bool CANFDComm::OpenTactile() {
  if (!WriteTactileControl(0x0100)) return false;

  bool should_start = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    tactile_poll_enabled_ = static_cast<bool>(tactile_cb_);
    should_start = tactile_poll_enabled_;
  }
  if (should_start) {
    EnsurePollStarted();
  }
  return true;
}

bool CANFDComm::CloseTactile() {
  bool ok = WriteTactileControl(0x0200);
  bool should_stop = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    tactile_poll_enabled_ = false;
    should_stop = !joints_poll_enabled_ && !hand_state_poll_enabled_ &&
                  !tactile_poll_enabled_;
  }
  if (should_stop) {
    StopPoll();
  }
  return ok;
}

bool CANFDComm::ZeroTactile() { return WriteTactileControl(0x0400); }

// Data retrieval

std::vector<Joint> CANFDComm::GetJoints() {
  if (!IsConnected() || config_.valid_joints.empty()) return {};

  uint16_t start_addr = 0;
  int count = 0;
  if (!GetJointInputSpan(config_, &start_addr, &count)) return {};

  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(start_addr, count, &bytes)) {
    GHAND_LOG_ERROR("Failed to read joints over CANFD");
    return {};
  }
  std::vector<uint16_t> regs;
  regs.reserve(bytes.size() / 2);
  for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
    regs.push_back(static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1]));
  }
  return ParseJoints(regs.data(), regs.size(), config_, start_addr);
}

HandState CANFDComm::GetHandInfo() {
  if (!IsConnected()) return HandState{};

  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(0x1021, 2, &bytes)) {
    GHAND_LOG_ERROR("Failed to read hand info over CANFD");
    return HandState{};
  }
  std::vector<uint16_t> regs;
  for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
    regs.push_back(static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1]));
  }
  return ParseHandInfo(regs.data(), regs.size());
}

TactileData CANFDComm::GetTactileData() {
  TactileData data;
  if (!IsConnected() || !config_.has_tactile) return data;

  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(0x1080, 16, &bytes)) {
    GHAND_LOG_ERROR("Failed to read tactile status over CANFD");
    return data;
  }
  std::vector<uint16_t> regs;
  for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
    regs.push_back(static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1]));
  }

  std::pair<uint8_t, uint8_t> tactile_err = ParseTactileStateError(regs[0]);
  data.sensor_state = tactile_err.first;
  data.sensor_error = tactile_err.second;

  int current_addr = 0x1080 + 16;
  for (size_t i = 0; i < config_.tactile_regions.size(); ++i) {
    const auto& region = config_.tactile_regions[i];
    RegionTactile rt;
    rt.region_name = region.id.c_str();
    rt.state = (tactile_err.first & (1 << static_cast<int>(i))) != 0;
    rt.resultant_force =
        ParseTactileResultant(regs.data(), static_cast<int>(i));

    int dist_regs = (region.sensor_count * 3 + 1) / 2;
    std::vector<uint8_t> dist_bytes;
    if (!ReadInputBytes(current_addr, dist_regs, &dist_bytes)) {
      GHAND_LOG_ERROR("Failed to read tactile region over CANFD");
      return data;
    }
    rt.distributed_forces = ParseTactileDistributed(
        dist_bytes.data(), dist_bytes.size(), region.sensor_count);
    current_addr += dist_regs;

    data.regions.push_back(std::move(rt));
  }
  return data;
}

// Callbacks

void CANFDComm::SetJointsCallback(JointsCallback cb) {
  bool should_start = false;
  bool should_stop = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    joints_cb_ = cb;
    if (connected_.load()) {
      joints_poll_enabled_ = static_cast<bool>(cb);
    }
    should_start = joints_poll_enabled_ && connected_.load();
    should_stop = !joints_poll_enabled_ && !hand_state_poll_enabled_ &&
                  !tactile_poll_enabled_;
  }
  if (should_start) {
    EnsurePollStarted();
  } else if (should_stop) {
    StopPoll();
  }
}

void CANFDComm::SetHandStateCallback(HandStateCallback cb) {
  bool should_start = false;
  bool should_stop = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    hand_state_cb_ = cb;
    if (connected_.load()) {
      hand_state_poll_enabled_ = static_cast<bool>(cb);
    }
    should_start = hand_state_poll_enabled_ && connected_.load();
    should_stop = !joints_poll_enabled_ && !hand_state_poll_enabled_ &&
                  !tactile_poll_enabled_;
  }
  if (should_start) {
    EnsurePollStarted();
  } else if (should_stop) {
    StopPoll();
  }
}

void CANFDComm::SetTactileDataCallback(TactileDataCallback cb) {
  bool should_start = false;
  bool should_stop = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    tactile_cb_ = cb;
    if (connected_.load()) {
      tactile_poll_enabled_ = static_cast<bool>(cb);
    }
    should_start = tactile_poll_enabled_ && connected_.load();
    should_stop = !joints_poll_enabled_ && !hand_state_poll_enabled_ &&
                  !tactile_poll_enabled_;
  }
  if (should_start) {
    EnsurePollStarted();
  } else if (should_stop) {
    StopPoll();
  }
}

// Firmware update

FirmwareUpdateError CANFDComm::BootUpdate(const std::string& filename,
                                          std::function<void(int)> progress) {
  (void)filename;
  (void)progress;
  GHAND_LOG_WARNING("BootUpdate not supported on CANFD");
  return FirmwareUpdateError::NOT_SUPPORTED;
}

bool CANFDComm::QueryFirmwareUpdateResults(FirmwareUpdateResults* results) {
  (void)results;
  return false;
}

// Polling subscription

void CANFDComm::EnsurePollStarted() {
  if (!poll_thread_.joinable()) {
    poll_stop_.store(false);
    poll_thread_ = std::thread(&CANFDComm::PollLoop, this);
  }
}

void CANFDComm::StopPoll() {
  poll_stop_.store(true);
  if (poll_thread_.joinable()) {
    poll_thread_.join();
  }
}

void CANFDComm::PollLoop() {
  auto last_tactile_time = std::chrono::steady_clock::time_point{};

  while (!poll_stop_.load()) {
    if (!connected_.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    JointsCallback joints_cb;
    HandStateCallback hand_state_cb;
    TactileDataCallback tactile_cb;
    bool joints_poll_enabled = false;
    bool hand_state_poll_enabled = false;
    bool tactile_poll_enabled = false;
    {
      std::lock_guard<std::mutex> lock(cb_mutex_);
      joints_cb = joints_cb_;
      hand_state_cb = hand_state_cb_;
      tactile_cb = tactile_cb_;
      joints_poll_enabled = joints_poll_enabled_;
      hand_state_poll_enabled = hand_state_poll_enabled_;
      tactile_poll_enabled = tactile_poll_enabled_;
    }

    if ((!joints_poll_enabled || !joints_cb) &&
        (!hand_state_poll_enabled || !hand_state_cb) &&
        (!tactile_poll_enabled || !tactile_cb)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    std::vector<Joint> joints;
    HandState hand_state;
    TactileData tactile_data;
    bool has_tactile_data = false;

    if (joints_poll_enabled && joints_cb && !config_.valid_joints.empty()) {
      joints = GetJoints();
    }
    if (hand_state_poll_enabled && hand_state_cb) {
      hand_state = GetHandInfo();
    }
    auto now = std::chrono::steady_clock::now();
    if (tactile_poll_enabled && tactile_cb &&
        (last_tactile_time.time_since_epoch().count() == 0 ||
         now - last_tactile_time >= std::chrono::milliseconds(100))) {
      tactile_data = GetTactileData();
      has_tactile_data = true;
      last_tactile_time = now;
    }

    if (joints_poll_enabled && joints_cb) joints_cb(joints);
    if (hand_state_poll_enabled && hand_state_cb) hand_state_cb(hand_state);
    if (tactile_poll_enabled && tactile_cb && has_tactile_data) {
      tactile_cb(tactile_data);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

}  // namespace internal
}  // namespace ghand
