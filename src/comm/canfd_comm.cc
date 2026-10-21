// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include "canfd_comm.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <exception>
#include <iomanip>
#include <sstream>
#include <thread>
#include <map>

#include "ghand/logging.h"
#include "logging_macros.h"
#include "modbus_codec.h"

namespace ghand {
namespace internal {

// CANFD valid data lengths (DLC mapping)
static const uint8_t kCanfdLengths[] = {0,  1,  2,  3,  4,  5,  6,  7,
                                         8,  12, 16, 20, 24, 32, 48, 64};
static const int kBaudrateConfigRegister = 0x0004;
static const int kSlaveIdRegister = 0x0003;
static const int kClearFaultRegister = 0x001F;
static const uint8_t kDefaultBaudrateGear = 0x05;

struct CANFDBaudrateConfig {
  uint32_t bitrate;
  uint8_t bitrate_sample_point;
  uint32_t dbitrate;
  uint8_t dbitrate_sample_point;
};

static bool ResolveCANFDBaudrateConfig(uint8_t baudrate_gear,
                                       CANFDBaudrateConfig* config) {
  if (config == nullptr) return false;
  switch (baudrate_gear) {
    case 0x00:
      *config = CANFDBaudrateConfig{500000, 80, 1000000, 75};
      return true;
    case 0x01:
      *config = CANFDBaudrateConfig{500000, 80, 2000000, 80};
      return true;
    case 0x02:
      *config = CANFDBaudrateConfig{500000, 80, 4000000, 80};
      return true;
    case 0x03:
      *config = CANFDBaudrateConfig{500000, 80, 5000000, 75};
      return true;
    case 0x04:
      *config = CANFDBaudrateConfig{1000000, 75, 4000000, 80};
      return true;
    case 0x05:
      *config = CANFDBaudrateConfig{1000000, 75, 5000000, 75};
      return true;
    default:
      return false;
  }
}

static uint8_t NearestCanfdLength(uint8_t len) {
  for (uint8_t vl : kCanfdLengths) {
    if (vl >= len) return vl;
  }
  return 64;
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
  if (!driver_) {
    tx_errors_++;
    return false;
  }

  canfd::Frame frame;
  frame.id = can_id;
  memcpy(frame.data, data, len);
  frame.len = NearestCanfdLength(len);
  frame.is_fd = true;
  frame.is_extended = true;

  if (driver_->Send(frame) == 0) {
    tx_packets_++;
    return true;
  }
  tx_errors_++;
  return false;
}

bool CANFDComm::RecvFrame(uint32_t* can_id, uint8_t* data, uint8_t* len,
                          int timeout_ms) {
  if (!driver_) {
    rx_errors_++;
    return false;
  }

  canfd::Frame frame;
  if (driver_->Receive(&frame, timeout_ms) != 0) return false;

  *can_id = frame.id;
  uint8_t copy_len = frame.len;
  if (copy_len > 64) copy_len = 64;
  memcpy(data, frame.data, copy_len);
  *len = copy_len;
  rx_packets_++;
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
      PackArbitration(src_id_, slave_id_, 0, func_code, 1, 1, 0, 0);
  if (!SendFrame(can_id, cmd_data, 4)) return false;

  // Gather response segments
  std::map<int, std::vector<uint8_t>> segments;
  auto start_time = std::chrono::steady_clock::now();

  while (true) {
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    int elapsed_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
            .count());
    if (elapsed_ms >= timeout_ms) {
      timeouts_++;
      return false;
    }

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

    if (r_ack != 1 || r_dst != src_id_ || r_src != slave_id_) continue;

    // Exception response?
    if (r_fc == (0x80 | func_code)) {
      rx_errors_++;
      return false;
    }
    if (r_fc != func_code) continue;

    // Single-frame response
    if (r_start == 1 && r_end == 1) {
      if (resp_len < count * 2 + 1) continue;
      out_bytes->assign(resp_data + 1, resp_data + 1 + count * 2);
      return true;
    }

    // Multi-frame response
    if ((r_start == 1 && r_seg != 0) || (r_seg == 0 && r_start != 1) ||
        r_toggle != r_seg % 2 || (r_seg > 0 && !segments.count(r_seg - 1))) continue;
    std::vector<uint8_t> seg_data(resp_data, resp_data + resp_len);
    if (r_seg == 0 && !seg_data.empty()) {
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
      if (segments.size() != static_cast<size_t>(r_seg + 1)) continue;
      if (out_bytes->size() < static_cast<size_t>(count * 2)) {
        segments.clear();
        continue;
      }
      out_bytes->resize(count * 2);
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

    uint32_t can_id = PackArbitration(src_id_, slave_id_, 0, func_code, start,
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
    if (elapsed_ms >= timeout_ms) {
      timeouts_++;
      return false;
    }

    uint32_t resp_id = 0;
    uint8_t resp_data[64];
    uint8_t resp_len = 0;
    int remaining = timeout_ms - elapsed_ms;
    if (!RecvFrame(&resp_id, resp_data, &resp_len, std::max(remaining, 10)))
      continue;

    int r_src = 0, r_dst = 0, r_ack = 0, r_fc = 0, r_start = 0, r_end = 0;
    UnpackArbitration(resp_id, &r_src, &r_dst, &r_ack, &r_fc, &r_start,
                       &r_end, nullptr, nullptr);

    if (r_ack != 1 || r_dst != src_id_ || r_src != slave_id_) continue;
    if (r_fc == (0x80 | func_code)) {
      rx_errors_++;
      return false;
    }
    if (r_fc == func_code && r_start == 1 && r_end == 1) {
      if (resp_len < 4) return false;
      return resp_data[0] == payload[0] && resp_data[1] == payload[1] &&
             resp_data[2] == payload[2] && resp_data[3] == payload[3];
    }
  }
}

bool CANFDComm::WriteSingleRegister(int addr, uint16_t value, int timeout_ms) {
  std::vector<uint8_t> data = {static_cast<uint8_t>((value >> 8) & 0xFF),
                                static_cast<uint8_t>(value & 0xFF)};
  return WriteRegisters(addr, data, timeout_ms);
}

bool CANFDComm::WaitInputResult(int addr, int baseline) {
  return WaitCommandMailbox([this, addr](uint16_t* value) {
    std::vector<uint8_t> bytes;
    if (!ReadInputBytes(addr, 1, &bytes) || bytes.size() < 2) return false;
    *value = static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
    return true;
  }, baseline);
}

// Connection management



bool CANFDComm::EstablishConnection(uint8_t slave_id, int timeout_ms) {
  const uint8_t candidates[] = {slave_id ? slave_id : static_cast<uint8_t>(0x31), 0x32};
  const int count = slave_id ? 1 : 2;
  auto connect = [this](uint8_t id) {
    slave_id_ = id;
    if (!WriteSingleRegister(0x0000, 1, 5000)) return false;
    if (!WaitConnectionState([this](uint16_t* value) {
      std::vector<uint8_t> bytes;
      if (!ReadInputBytes(0x1047, 1, &bytes) || bytes.size() < 2) return false;
      *value = static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
      return true;
    })) return false;
    return WaitInputResult(0x1026);
  };
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    uint32_t id = 0; uint8_t bytes[64] = {}; uint8_t len = 0;
    if (!RecvFrame(&id, bytes, &len, 50)) continue;
    int src, dst, ack, fc;
    UnpackArbitration(id, &src, &dst, &ack, &fc, nullptr, nullptr, nullptr, nullptr);
    if (ack == 0 && fc == 0x44 && src == dst) {
      for (int i = 0; i < count; ++i) if (src == candidates[i]) return connect(candidates[i]);
    }
  }
  for (int i = 0; i < count; ++i) {
    uint8_t target = candidates[i];
    uint8_t probe[8] = {0, 0, 0, 1, target, 0, 0, 0};
    if (!SendFrame(PackArbitration(src_id_, target, 0, 0x44), probe, 8)) continue;
    deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
      uint32_t id = 0; uint8_t bytes[64] = {}; uint8_t len = 0;
      if (!RecvFrame(&id, bytes, &len, 50)) continue;
      int src, dst, ack, fc;
      UnpackArbitration(id, &src, &dst, &ack, &fc, nullptr, nullptr, nullptr, nullptr);
      if (ack != 1 || dst != src_id_ || src != target) continue;
      if (fc == 0xC4) break;
      if (fc == 0x44) {
        if (connect(target)) return true;
        break;
      }
    }
  }
  return false;
}

void CANFDComm::DeleteConnection() {
  WriteSingleRegister(0x0000, 2, 2000);
}

int CANFDComm::Connect(const std::string& device_name, uint8_t slave_id,
                       uint8_t baudrate_gear) {
  if (IsConnected()) return 0;
  if (!driver_) {
    GHAND_LOG_ERROR("CANFD driver not available");
    return -1;
  }
  if (slave_id > 0x3F) {
    GHAND_LOG_ERROR("Invalid CANFD slave ID: 0x"
                    << std::hex << static_cast<int>(slave_id) << std::dec);
    return -1;
  }

  CANFDBaudrateConfig baudrate_config = {};
  if (!ResolveCANFDBaudrateConfig(baudrate_gear, &baudrate_config)) {
    GHAND_LOG_ERROR("Invalid CANFD baudrate gear: 0x"
                    << std::hex << static_cast<int>(baudrate_gear)
                    << std::dec);
    return -1;
  }

  if (driver_->Open(device_name, baudrate_config.bitrate,
                    baudrate_config.dbitrate,
                    baudrate_config.bitrate_sample_point,
                    baudrate_config.dbitrate_sample_point) != 0) {
    GHAND_LOG_ERROR("Failed to open CANFD channel: " << device_name);
    return -2;
  }

  connected_.store(true);

  if (!EstablishConnection(slave_id, 500)) {
    GHAND_LOG_ERROR("Failed to establish CANFD connection");
    driver_->Close();
    connected_.store(false);
    return -3;
  }

  joint_modes_.clear();
  hand_mode_ = ControlMode::POSITION;
  keepalive_stop_.store(false);
  keepalive_thread_ = std::thread(&CANFDComm::KeepaliveLoop, this);
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
  return 0;
}

int CANFDComm::Disconnect() {
  keepalive_stop_.store(true);
  if (keepalive_thread_.joinable()) keepalive_thread_.join();
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

  return 0;
}

std::map<std::string, std::string> CANFDComm::SearchAdapters() {
  if (driver_) {
    return driver_->EnumerateAdapters();
  }
  return {};
}

void CANFDComm::UpdateConfig(const ProductConfig& config) {
  std::lock_guard<std::mutex> lock(command_mutex_);
  config_ = config;
  joint_modes_.clear();
  hand_mode_ = ControlMode::POSITION;
}

bool CANFDComm::SetSlaveId(uint8_t slave_id) {
  if (!IsConnected() || slave_id == 0 || slave_id > 0x3F) return false;
  if (!WriteSingleRegister(kSlaveIdRegister, (slave_id << 8) | 1)) return false;
  uint8_t old_id = slave_id_;
  slave_id_ = slave_id;

  if (WaitInputResult(0x1029)) return true;
  slave_id_ = old_id;
  return false;
}

bool CANFDComm::SetBaudrateConfig(uint8_t baudrate_gear) {
  if (!IsConnected() || baudrate_gear > 5) return false;
  if (!WriteSingleRegister(kBaudrateConfigRegister, (baudrate_gear << 8) | 1)) return false;
  return WaitInputResult(0x102A);
}

Diagnostics CANFDComm::GetDiagnostics() const {
  Diagnostics diagnostics;
  diagnostics.connection.connected = IsConnected();
  diagnostics.connection.comm_type = CommType::CANFD;
  diagnostics.transport.tx_packets = tx_packets_.load();
  diagnostics.transport.rx_packets = rx_packets_.load();
  diagnostics.transport.tx_errors = tx_errors_.load();
  diagnostics.transport.rx_errors = rx_errors_.load();
  diagnostics.transport.timeouts = timeouts_.load();
  diagnostics.subscription.callback_errors = callback_errors_.load();
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    diagnostics.subscription.active_subscriptions =
        (joints_cb_ ? 1 : 0) + (hand_state_cb_ ? 1 : 0) +
        (tactile_cb_ ? 1 : 0);
  }
  return diagnostics;
}

// Device info

bool CANFDComm::ReadInputBytes(int addr, int count,
                               std::vector<uint8_t>* bytes) {
  if (bytes == nullptr) return false;
  return ReadRegisters(addr, count, bytes, 0x04);
}

DeviceInfo CANFDComm::GetDeviceInfo() {
  DeviceInfo info;
  if (!IsConnected()) return info;
  std::vector<uint8_t> bytes;
  if (ReadInputBytes(0x100A, 8, &bytes)) info.device_name = ParseDeviceName(bytes.data(), bytes.size());
  if (ReadInputBytes(0x1000, 10, &bytes)) info.serial_number = ParseAsciiSerialNumber(bytes.data(), bytes.size());
  auto version = [this](int addr) -> std::string {
    std::vector<uint8_t> bytes;
    if (!ReadInputBytes(addr, 1, &bytes) || bytes.size() < 2) return "N/A";
    return ParsePackedFirmwareVersion(static_cast<uint16_t>((bytes[0] << 8) | bytes[1]));
  };
  info.software_version = version(0x1019);
  info.position_sensor_version = version(0x101A);
  info.tactile_sensor_version = version(0x101B);
  info.motor_driver_version = version(0x101C);
  info.firmware_package_version = version(0x101D);
  return info;
}

HandType CANFDComm::GetHandType() {
  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(0x1012, 1, &bytes)) {
    GHAND_LOG_ERROR("Failed to read hand type over CANFD");
    return HandType::NONE;
  }
  uint8_t type = ParseHandType(bytes.data(), bytes.size());
  if (type == 1) return HandType::LEFT;
  if (type == 2) return HandType::RIGHT;
  return HandType::NONE;
}

// Motion control

bool CANFDComm::MoveJoints(const std::vector<JointCommand>& joints, ControlMode mode) {
  if (!IsConnected() || joints.empty()) return false;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  auto data = EncodeHandCommand(joints, mode, config_, joint_modes_);
  if (!WriteRegisters(config_.mode_register, RegistersToBytes(data))) return false;
  hand_mode_ = mode;
  for (const auto& joint : joints) {
    if (!config_.joint_control_registers.count(joint.id)) continue;
    auto previous = joint_modes_.find(joint.id);
    ControlMode fallback = previous == joint_modes_.end() ? ControlMode::POSITION : previous->second;
    joint_modes_[joint.id] = static_cast<ControlMode>(EncodeJointCommand(joint, mode, fallback)[0] & 0x0F);
  }
  return true;
}

void CANFDComm::Stop() {
  if (!IsConnected()) return;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  if (!WriteSingleRegister(config_.mode_register, 0x0100 | static_cast<uint8_t>(hand_mode_))) return;
  for (const auto& entry : config_.joint_control_registers) {
    uint16_t value = 0x0200 | static_cast<uint8_t>(joint_modes_[entry.first]);
    if (!WriteSingleRegister(entry.second, value)) return;
  }
}

bool CANFDComm::DisableHand() {
  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  return WriteSingleRegister(config_.mode_register, static_cast<uint8_t>(hand_mode_));
}

// System operations

bool CANFDComm::ClearFault() {
  if (!IsConnected()) return false;
  int previous = -1;
  std::vector<uint8_t> bytes; if (ReadInputBytes(0x1027, 1, &bytes) && bytes.size() >= 2) previous = (bytes[0] << 8) | bytes[1];
  if (!WriteSingleRegister(kClearFaultRegister, 0x0001)) return false;
  return WaitInputResult(0x1027, previous);
}

bool CANFDComm::InitJoint() {
  GHAND_LOG_ERROR("InitJoint is not supported by the new protocol");
  return false;
}

SelfTestErrorInfo CANFDComm::GetSelfTestErrorInfo() {
  GHAND_LOG_ERROR("Legacy self-test commands are not supported by the new protocol");
  return SelfTestErrorInfo{};
}

uint8_t CANFDComm::GetSelfTestStatus() { return 0; }

// Tactile sensor

bool CANFDComm::WriteTactileControl(uint16_t command) {
  if (!IsConnected()) return false;
  int previous = -1;
  std::vector<uint8_t> bytes; if (ReadInputBytes(0x1028, 1, &bytes) && bytes.size() >= 2) previous = (bytes[0] << 8) | bytes[1];
  if (!WriteSingleRegister(config_.tactile_control_register, command)) return false;
  return WaitInputResult(0x1028, previous);
}

bool CANFDComm::OpenTactile() {
  if (!WriteTactileControl(0x0001)) return false;

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
  bool ok = WriteTactileControl(0x0002);
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

bool CANFDComm::ZeroTactile() { return WriteTactileControl(0x0004); }

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
  if (!ReadInputBytes(0x1051, 14, &bytes)) {
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
  if (!ReadInputBytes(0x111A, 16, &bytes)) {
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

  int current_addr = 0x111A + 16;
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

void CANFDComm::KeepaliveLoop() {
  while (!keepalive_stop_.load()) {
    for (int i = 0; i < 10 && !keepalive_stop_.load(); ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (keepalive_stop_.load()) break;
    std::vector<uint8_t> bytes;
    ReadInputBytes(0x1047, 1, &bytes);
  }
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

    try {
      if (joints_poll_enabled && joints_cb) joints_cb(joints);
    } catch (const std::exception& e) {
      callback_errors_++;
      GHAND_LOG_ERROR("CANFD joints callback failed: " << e.what());
    } catch (...) {
      callback_errors_++;
      GHAND_LOG_ERROR("CANFD joints callback failed with unknown exception");
    }

    try {
      if (hand_state_poll_enabled && hand_state_cb) hand_state_cb(hand_state);
    } catch (const std::exception& e) {
      callback_errors_++;
      GHAND_LOG_ERROR("CANFD hand-state callback failed: " << e.what());
    } catch (...) {
      callback_errors_++;
      GHAND_LOG_ERROR(
          "CANFD hand-state callback failed with unknown exception");
    }

    try {
      if (tactile_poll_enabled && tactile_cb && has_tactile_data) {
        tactile_cb(tactile_data);
      }
    } catch (const std::exception& e) {
      callback_errors_++;
      GHAND_LOG_ERROR("CANFD tactile callback failed: " << e.what());
    } catch (...) {
      callback_errors_++;
      GHAND_LOG_ERROR("CANFD tactile callback failed with unknown exception");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

bool CANFDComm::GetHandStateMachine(HandStateMachine* state) {
  if (!IsConnected() || state == nullptr) return false;
  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(0x1048, 1, &bytes) || bytes.size() < 2) return false;
  *state = static_cast<HandStateMachine>(bytes[1]);
  return true;
}

}  // namespace internal
}  // namespace ghand
