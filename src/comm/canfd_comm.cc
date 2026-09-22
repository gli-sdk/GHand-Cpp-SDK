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
#include "self_test_workflow.h"

namespace ghand {
namespace internal {

// CANFD valid data lengths (DLC mapping)
static const uint8_t kCanfdLengths[] = {0,  1,  2,  3,  4,  5,  6,  7,
                                         8,  12, 16, 20, 24, 32, 48, 64};
static const int kBaudrateConfigRegister = 0x002C;
static const uint8_t kDefaultBaudrateGear = 0x05;
// Slave address used when a bus-level operation runs before enumeration.
static const uint8_t kDefaultSlaveAddress = 0x31;
// A broadcast frame is never acknowledged, so the write only waits for the
// frame to leave the adapter.
static const int kBroadcastTimeoutMs = 500;
// The hands need time to apply a broadcast before the next request (feedback
// polling) hits the bus. Tune on hardware if a hand drops frames right after.
static const int kBroadcastTurnaroundMs = 3;
static const int kTactilePollIntervalMs = 100;
// A hand that fails this many consecutive polls is skipped for a while so the
// other hand on the bus keeps its polling rate.
static const int kOfflineFailThreshold = 3;
static const int kOfflineBackoffMs = 2000;

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

bool CANFDComm::ReadRegisters(uint8_t slave, int addr, int count,
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
      PackArbitration(src_id_, slave, 0, func_code, 1, 1, 0, 0);
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

    if (r_ack != 1 || r_dst != src_id_ || r_src != slave) continue;

    // Exception response?
    if (r_fc == (0x80 | func_code)) {
      rx_errors_++;
      return false;
    }
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

bool CANFDComm::WriteRegisters(uint8_t slave, int addr,
                               const std::vector<uint8_t>& data,
                               int timeout_ms, bool expect_response) {
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

    uint32_t can_id = PackArbitration(src_id_, slave, 0, func_code, start,
                                      end, toggle, seg_idx);
    if (!SendFrame(can_id, payload.data() + offset, seg_len)) return false;
  }

  // A broadcast frame is never acknowledged, so there is nothing to wait for.
  if (!expect_response) return true;

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

    if (r_ack != 1 || r_dst != src_id_ || r_src != slave) continue;
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

bool CANFDComm::WriteSingleRegister(uint8_t slave, int addr, uint16_t value,
                                    int timeout_ms) {
  std::vector<uint8_t> data = {static_cast<uint8_t>((value >> 8) & 0xFF),
                                static_cast<uint8_t>(value & 0xFF)};
  return WriteRegisters(slave, addr, data, timeout_ms);
}

bool CANFDComm::WaitHoldingResult(uint8_t slave, int addr, int timeout_ms,
                                  int interval_ms) {
  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    std::vector<uint8_t> bytes;
    if (ReadRegisters(slave, addr, 1, &bytes, 0x03) && bytes.size() >= 2) {
      uint16_t reg =
          (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
      uint8_t status = static_cast<uint8_t>(reg & 0x00FF);
      if (status == 1) return true;
      if (status == 2) return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
  }
  return false;
}

// Connection management

bool CANFDComm::NodeIdDetection(uint8_t slave, int timeout_ms) {
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
      if (resp_data[0] == slave) {
        GHAND_LOG_INFO("Detected slave Node ID 0x"
                       << std::hex << static_cast<int>(slave));
        return true;
      }
    }
  }
  return false;
}

bool CANFDComm::TryConnectSlave(
    uint8_t target, const std::vector<std::vector<uint8_t>>& payloads,
    int timeout_ms) {
  for (const auto& data : payloads) {
    uint32_t can_id = PackArbitration(src_id_, target, 0, 0x02, 1, 1, 0, 0);
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

      if (r_ack != 1 || r_dst != src_id_ || r_src != target) continue;
      if (r_fc == 0x82) {
        break;  // Exception, try next payload
      }
      if (r_fc == 0x02) return true;
    }
  }
  return false;
}

bool CANFDComm::EstablishConnection(uint8_t slave_id, int timeout_ms) {
  const uint8_t candidates[] = {
      slave_id == 0 ? static_cast<uint8_t>(0x31) : slave_id,
      static_cast<uint8_t>(0x32)};
  // An explicit slave ID connects that hand only; 0 enumerates the bus.
  const int candidate_count = slave_id == 0 ? 2 : 1;

  auto payloads = BuildConnectionPayloads(config_, false);

  std::vector<ConnectedHand> hands;
  for (int i = 0; i < candidate_count; ++i) {
    const uint8_t target = candidates[i];
    if (!TryConnectSlave(target, payloads, timeout_ms)) continue;

    ConnectedHand hand;
    hand.slave_index = target;
    hand.hand_type = GetHandTypeForSlave(target);
    hand.device_info = GetDeviceInfoForSlave(target);
    hands.push_back(std::move(hand));
  }
  if (hands.empty()) return false;

  {
    std::lock_guard<std::mutex> lock(hands_mutex_);
    hands_ = std::move(hands);
    health_.clear();
  }
  return true;
}

std::vector<ConnectedHand> CANFDComm::GetConnectedHands() {
  std::lock_guard<std::mutex> lock(hands_mutex_);
  return hands_;
}

bool CANFDComm::TargetSlaves(uint16_t slave_index,
                             std::vector<uint8_t>* slaves) {
  if (slaves == nullptr) return false;
  slaves->clear();

  std::lock_guard<std::mutex> lock(hands_mutex_);
  if (slave_index == 0) {
    for (const auto& hand : hands_) {
      slaves->push_back(static_cast<uint8_t>(hand.slave_index));
    }
    return !slaves->empty();
  }

  if (slave_index > 0xFF) return false;
  const uint8_t target = static_cast<uint8_t>(slave_index);
  for (const auto& hand : hands_) {
    if (hand.slave_index == target) {
      slaves->push_back(target);
      return true;
    }
  }
  GHAND_LOG_ERROR("CANFD slave 0x" << std::hex << static_cast<int>(target)
                                   << std::dec << " is not on the bus");
  return false;
}

void CANFDComm::DeleteConnection() {
  auto payloads = BuildConnectionPayloads(config_, true);

  std::vector<uint8_t> targets;
  {
    std::lock_guard<std::mutex> lock(hands_mutex_);
    for (const auto& hand : hands_) targets.push_back(hand.slave_index);
  }
  if (targets.empty()) targets.push_back(kDefaultSlaveAddress);

  for (uint8_t target : targets) {
    for (const auto& data : payloads) {
      uint32_t can_id = PackArbitration(src_id_, target, 0, 0x05, 1, 1, 0, 0);
      SendFrame(can_id, data.data(), static_cast<uint8_t>(data.size()));
    }
  }
}

int CANFDComm::Connect(const std::string& device_name, uint8_t slave_id,
                       uint8_t baudrate_gear) {
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
  {
    std::lock_guard<std::mutex> lock(hands_mutex_);
    hands_.clear();
    health_.clear();
  }
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

void CANFDComm::UpdateConfig(const ProductConfig& config) { config_ = config; }

bool CANFDComm::SetSlaveId(uint16_t slave_index, uint8_t new_slave_id) {
  if (!IsConnected()) return false;
  // A rename always targets exactly one hand: address 0 is the broadcast
  // address and the Connect() scan sentinel, and renaming every hand at once
  // would collide on the new ID.
  if (slave_index == 0 || slave_index > 0xFF) return false;
  if (new_slave_id == 0 || new_slave_id > 0x3F) {
    GHAND_LOG_ERROR("Invalid CANFD slave ID: 0x"
                    << std::hex << static_cast<int>(new_slave_id) << std::dec);
    return false;
  }

  const uint8_t old_id = static_cast<uint8_t>(slave_index);
  {
    std::lock_guard<std::mutex> lock(hands_mutex_);
    bool known = false;
    for (const auto& hand : hands_) {
      if (hand.slave_index == old_id) {
        known = true;
        break;
      }
    }
    if (!known) {
      GHAND_LOG_ERROR("CANFD slave 0x" << std::hex
                                       << static_cast<int>(old_id) << std::dec
                                       << " is not on the bus");
      return false;
    }
    if (new_slave_id != old_id) {
      for (const auto& hand : hands_) {
        if (hand.slave_index == new_slave_id) {
          GHAND_LOG_ERROR("CANFD slave ID 0x"
                          << std::hex << static_cast<int>(new_slave_id) << std::dec
                          << " is already used by another hand");
          return false;
        }
      }
    }
  }

  if (!WriteSingleRegister(old_id, 0x0000, new_slave_id)) return false;

  {
    std::lock_guard<std::mutex> lock(hands_mutex_);
    for (auto& hand : hands_) {
      if (hand.slave_index == old_id) hand.slave_index = new_slave_id;
    }
    health_.erase(old_id);
  }
  GHAND_LOG_INFO("CANFD slave ID set to 0x"
                 << std::hex << static_cast<int>(new_slave_id) << std::dec);
  return true;
}

bool CANFDComm::SetBaudrateConfig(uint16_t slave_index,
                                  uint8_t baudrate_gear) {
  if (!IsConnected()) return false;

  CANFDBaudrateConfig baudrate_config = {};
  if (!ResolveCANFDBaudrateConfig(baudrate_gear, &baudrate_config)) {
    GHAND_LOG_ERROR("Invalid CANFD baudrate gear: 0x"
                    << std::hex << static_cast<int>(baudrate_gear)
                    << std::dec);
    return false;
  }

  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return false;

  for (uint8_t slave : targets) {
    if (!WriteSingleRegister(slave, kBaudrateConfigRegister, baudrate_gear)) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  GHAND_LOG_INFO("CANFD baudrate config set to gear 0x"
                 << std::hex << static_cast<int>(baudrate_gear) << std::dec
                 << " (bitrate=" << baudrate_config.bitrate << "/"
                 << static_cast<int>(baudrate_config.bitrate_sample_point)
                 << "%, dbitrate=" << baudrate_config.dbitrate << "/"
                 << static_cast<int>(baudrate_config.dbitrate_sample_point)
                 << "%, effective after next power-up)");
  return true;
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

bool CANFDComm::ReadInputBytes(uint8_t slave, int addr, int count,
                               std::vector<uint8_t>* bytes) {
  if (bytes == nullptr) return false;
  return ReadRegisters(slave, addr, count, bytes, 0x04);
}

DeviceInfo CANFDComm::GetDeviceInfo(uint16_t slave_index) {
  if (slave_index == 0 || slave_index > 0xFF) return DeviceInfo{};
  return GetDeviceInfoForSlave(static_cast<uint8_t>(slave_index));
}

DeviceInfo CANFDComm::GetDeviceInfoForSlave(uint8_t slave) {
  DeviceInfo info;
  std::vector<uint8_t> bytes;
  auto read_version = [this, slave](int addr,
                                    const char* name) -> std::string {
    std::vector<uint8_t> version_bytes;
    if (!ReadInputBytes(slave, addr, 1, &version_bytes) ||
        version_bytes.size() < 2) {
      return "N/A";
    }
    uint16_t raw =
        (static_cast<uint16_t>(version_bytes[0]) << 8) | version_bytes[1];
    if (raw == 0) {
      return "N/A";
    }
    return ParsePackedFirmwareVersion(raw);
  };

  if (ReadInputBytes(slave, 0x1000, 8, &bytes)) {
    info.device_name = ParseDeviceName(bytes.data(), bytes.size());
    if (info.device_name.empty()) {
      info.device_name = "N/A";
    }
  }

  if (ReadInputBytes(slave, 0x1008, 8, &bytes)) {
    info.hardware_version = ParseHardwareVersion(bytes.data(), bytes.size());
    if (info.hardware_version.empty()) {
      info.hardware_version = "N/A";
    }
  }

  if (ReadInputBytes(slave, 0x1010, 8, &bytes)) {
    info.software_version = ParseFirmwareVersion(bytes.data(), bytes.size());
    if (info.software_version.empty()) {
      info.software_version = "N/A";
    }
  }

  if (ReadInputBytes(slave, 0x119A, 10, &bytes)) {
    std::string serial = ParseAsciiSerialNumber(bytes.data(), bytes.size());
    if (!serial.empty()) {
      info.serial_number = serial;
    }
  }

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

HandType CANFDComm::GetHandType(uint16_t slave_index) {
  if (slave_index == 0 || slave_index > 0x3F) return HandType::NONE;
  return GetHandTypeForSlave(static_cast<uint8_t>(slave_index));
}

HandType CANFDComm::GetHandTypeForSlave(uint8_t slave) {
  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(slave, 0x1020, 1, &bytes)) {
    GHAND_LOG_ERROR("Failed to read hand type over CANFD");
    return HandType::NONE;
  }
  uint8_t type = ParseHandType(bytes.data(), bytes.size());
  if (type == 1) return HandType::LEFT;
  if (type == 2) return HandType::RIGHT;
  return HandType::NONE;
}

// Motion control

bool CANFDComm::MoveJoints(uint16_t slave_index,
                           const std::vector<JointCommand>& joints,
                           ControlMode mode) {
  if (!IsConnected() || joints.empty()) return false;

  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return false;

  uint16_t start_addr = 0;
  std::vector<uint16_t> block;
  if (!BuildJointControlBlock(config_, joints, mode, &start_addr, &block)) {
    GHAND_LOG_ERROR("Failed to encode the joint control block");
    return false;
  }
  const auto bytes = RegistersToBytes(block);

  if (targets.size() > 1) {
    // One frame for the whole bus: a broadcast is never acknowledged, so the
    // feedback polling is what confirms the hands actually moved.
    if (!WriteRegisters(0x00, start_addr, bytes, kBroadcastTimeoutMs,
                        /*expect_response=*/false)) {
      return false;
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds(kBroadcastTurnaroundMs));
    return true;
  }

  return WriteRegisters(targets.front(), start_addr, bytes);
}

void CANFDComm::Stop(uint16_t slave_index) {
  if (!IsConnected()) return;
  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return;

  for (uint8_t slave : targets) {
    if (config_.stop_register >= 0) {
      WriteSingleRegister(slave, config_.stop_register, 0x0001);
      continue;
    }

    const auto& control_map = config_.joint_control_registers.empty()
                                  ? kHoldingRegMap
                                  : config_.joint_control_registers;
    for (JointId joint_id : config_.valid_joints) {
      auto it = control_map.find(joint_id);
      if (it != control_map.end()) {
        WriteSingleRegister(slave, it->second, 0x0001);
      }
    }
  }
}

// System operations

bool CANFDComm::ClearFault(uint16_t slave_index) {
  if (!IsConnected()) return false;
  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return false;

  bool ok = true;
  for (uint8_t slave : targets) {
    if (!WriteSingleRegister(slave, 0x0001, 0x0100)) {
      ok = false;
      continue;
    }
    if (!WaitHoldingResult(slave, 0x0001)) {
      GHAND_LOG_ERROR("Fault clearance failed or timed out on CANFD slave 0x"
                      << std::hex << static_cast<int>(slave) << std::dec);
      ok = false;
    }
  }
  if (ok) GHAND_LOG_INFO("Fault cleared");
  return ok;
}

SelfTestErrorInfo CANFDComm::GetSelfTestErrorInfo(uint16_t slave_index) {
  if (!IsConnected() || slave_index == 0 || slave_index > 0xFF) {
    return SelfTestErrorInfo{};
  }

  const uint8_t slave = static_cast<uint8_t>(slave_index);
  SelfTestRegisterIO io;
  io.read_registers =
      [this, slave](uint16_t address, int count) -> std::vector<uint8_t> {
    std::vector<uint8_t> bytes;
    if (!ReadRegisters(slave, address, count, &bytes, 0x03)) {
      return {};
    }
    if (static_cast<int>(bytes.size()) < count * 2) {
      return {};
    }
    return bytes;
  };
  io.write_register = [this, slave](uint16_t address, uint16_t value) -> bool {
    return WriteSingleRegister(slave, address, value);
  };
  return BuildSelfTestErrorInfo(config_, io);
}

uint8_t CANFDComm::GetSelfTestStatus(uint16_t slave_index) {
  if (!IsConnected() || slave_index == 0 || slave_index > 0xFF) return 0;
  std::vector<uint8_t> bytes;
  if (!ReadRegisters(static_cast<uint8_t>(slave_index), 0x0038, 1, &bytes,
                     0x03) ||
      bytes.size() < 2) {
    return 0;
  }
  return bytes[1];
}

// Tactile sensor

bool CANFDComm::WriteTactileControl(uint8_t slave, uint16_t command) {
  if (!IsConnected()) return false;
  return WriteSingleRegister(slave, config_.tactile_control_register, command);
}

bool CANFDComm::OpenTactile(uint16_t slave_index) {
  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return false;

  for (uint8_t slave : targets) {
    if (!WriteTactileControl(slave, 0x0100)) return false;
  }

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

bool CANFDComm::CloseTactile(uint16_t slave_index) {
  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return false;

  bool ok = true;
  for (uint8_t slave : targets) {
    ok = WriteTactileControl(slave, 0x0200) && ok;
  }

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

bool CANFDComm::ZeroTactile(uint16_t slave_index) {
  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return false;

  bool ok = true;
  for (uint8_t slave : targets) {
    ok = WriteTactileControl(slave, 0x0400) && ok;
  }
  return ok;
}

// Data retrieval

std::vector<Joint> CANFDComm::GetJoints(uint8_t slave, bool* ok) {
  if (ok != nullptr) *ok = false;
  if (!IsConnected() || config_.valid_joints.empty()) return {};

  uint16_t start_addr = 0;
  int count = 0;
  if (!GetJointInputSpan(config_, &start_addr, &count)) return {};

  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(slave, start_addr, count, &bytes)) {
    GHAND_LOG_ERROR("Failed to read joints over CANFD");
    return {};
  }
  std::vector<uint16_t> regs;
  regs.reserve(bytes.size() / 2);
  for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
    regs.push_back(static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1]));
  }
  if (ok != nullptr) *ok = true;
  return ParseJoints(regs.data(), regs.size(), config_, start_addr);
}

HandState CANFDComm::GetHandInfo(uint8_t slave, bool* ok) {
  if (ok != nullptr) *ok = false;
  if (!IsConnected()) return HandState{};

  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(slave, 0x1021, 2, &bytes)) {
    GHAND_LOG_ERROR("Failed to read hand info over CANFD");
    return HandState{};
  }
  std::vector<uint16_t> regs;
  for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
    regs.push_back(static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1]));
  }
  if (ok != nullptr) *ok = true;
  return ParseHandInfo(regs.data(), regs.size());
}

TactileData CANFDComm::GetTactileData(uint8_t slave, bool* ok) {
  if (ok != nullptr) *ok = false;
  TactileData data;
  if (!IsConnected() || !config_.has_tactile) return data;

  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(slave, 0x1080, 16, &bytes)) {
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
    if (!ReadInputBytes(slave, current_addr, dist_regs, &dist_bytes)) {
      GHAND_LOG_ERROR("Failed to read tactile region over CANFD");
      return data;
    }
    rt.distributed_forces = ParseTactileDistributed(
        dist_bytes.data(), dist_bytes.size(), region.sensor_count);
    current_addr += dist_regs;

    data.regions.push_back(std::move(rt));
  }
  if (ok != nullptr) *ok = true;
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

    std::vector<ConnectedHand> hands;
    {
      std::lock_guard<std::mutex> lock(hands_mutex_);
      hands = hands_;
    }
    if (hands.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    const bool wants_joints =
        joints_poll_enabled && joints_cb && !config_.valid_joints.empty();
    const bool wants_state = hand_state_poll_enabled && hand_state_cb;
    const bool wants_tactile = tactile_poll_enabled && tactile_cb;

    for (const auto& hand : hands) {
      const uint8_t slave = static_cast<uint8_t>(hand.slave_index);
      const auto now = std::chrono::steady_clock::now();

      bool backing_off = false;
      bool tactile_due = false;
      {
        std::lock_guard<std::mutex> lock(hands_mutex_);
        HandHealth& health = health_[slave];
        backing_off = now < health.skip_until;
        if (!backing_off && wants_tactile) {
          tactile_due =
              health.last_tactile_time.time_since_epoch().count() == 0 ||
              now - health.last_tactile_time >=
                  std::chrono::milliseconds(kTactilePollIntervalMs);
          if (tactile_due) health.last_tactile_time = now;
        }
      }
      if (backing_off) continue;

      bool read_ok = true;
      bool ok = true;

      std::vector<Joint> joints;
      if (wants_joints) {
        joints = GetJoints(slave, &ok);
        read_ok = read_ok && ok;
      }

      HandState hand_state;
      if (wants_state) {
        hand_state = GetHandInfo(slave, &ok);
        read_ok = read_ok && ok;
      }

      TactileData tactile_data;
      if (tactile_due) {
        tactile_data = GetTactileData(slave, &ok);
        read_ok = read_ok && ok;
      }

      {
        std::lock_guard<std::mutex> lock(hands_mutex_);
        HandHealth& health = health_[slave];
        if (!read_ok) {
          if (++health.fail_count >= kOfflineFailThreshold) {
            health.skip_until =
                std::chrono::steady_clock::now() +
                std::chrono::milliseconds(kOfflineBackoffMs);
            GHAND_LOG_WARNING("CANFD hand 0x"
                              << std::hex << static_cast<int>(slave)
                              << std::dec << " not responding; backing off "
                              << kOfflineBackoffMs << "ms");
          }
        } else {
          health.fail_count = 0;
          health.skip_until = std::chrono::steady_clock::time_point{};
        }
      }

      try {
        if (wants_joints) joints_cb(slave, joints);
      } catch (const std::exception& e) {
        callback_errors_++;
        GHAND_LOG_ERROR("CANFD joints callback failed: " << e.what());
      } catch (...) {
        callback_errors_++;
        GHAND_LOG_ERROR("CANFD joints callback failed with unknown exception");
      }

      try {
        if (wants_state) hand_state_cb(slave, hand_state);
      } catch (const std::exception& e) {
        callback_errors_++;
        GHAND_LOG_ERROR("CANFD hand-state callback failed: " << e.what());
      } catch (...) {
        callback_errors_++;
        GHAND_LOG_ERROR(
            "CANFD hand-state callback failed with unknown exception");
      }

      try {
        if (tactile_due) tactile_cb(slave, tactile_data);
      } catch (const std::exception& e) {
        callback_errors_++;
        GHAND_LOG_ERROR("CANFD tactile callback failed: " << e.what());
      } catch (...) {
        callback_errors_++;
        GHAND_LOG_ERROR("CANFD tactile callback failed with unknown exception");
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

}  // namespace internal
}  // namespace ghand
