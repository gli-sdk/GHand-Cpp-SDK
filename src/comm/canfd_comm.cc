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

struct CANFDBaudrateConfig {
  uint32_t bitrate;
  uint8_t bitrate_sample_point;
  uint32_t dbitrate;
  uint8_t dbitrate_sample_point;
};

static bool ResolveCANFDBaudrateConfig(uint8_t baudrate_gear,
                                       CANFDBaudrateConfig* config) {
  if (config == nullptr) return false;
  switch (static_cast<CANFDBitTiming>(baudrate_gear)) {
    case CANFDBitTiming::TIMING_500K_800K:
      *config = CANFDBaudrateConfig{500000, 80, 800000, 80};
      return true;
    case CANFDBitTiming::TIMING_500K_1M:
      *config = CANFDBaudrateConfig{500000, 80, 1000000, 75};
      return true;
    case CANFDBitTiming::TIMING_500K_2M:
      *config = CANFDBaudrateConfig{500000, 80, 2000000, 80};
      return true;
    case CANFDBitTiming::TIMING_500K_4M:
      *config = CANFDBaudrateConfig{500000, 80, 4000000, 80};
      return true;
    case CANFDBitTiming::TIMING_1M_1M:
      *config = CANFDBaudrateConfig{1000000, 75, 1000000, 75};
      return true;
    case CANFDBitTiming::TIMING_1M_2M:
      *config = CANFDBaudrateConfig{1000000, 75, 2000000, 80};
      return true;
    case CANFDBitTiming::TIMING_1M_4M:
      *config = CANFDBaudrateConfig{1000000, 75, 4000000, 80};
      return true;
    case CANFDBitTiming::TIMING_1M_5M:
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
    return false;
  }

  canfd::Frame frame;
  frame.id = can_id;
  memset(frame.data, 0, sizeof(frame.data));
  memcpy(frame.data, data, len);
  frame.len = NearestCanfdLength(len);
  frame.is_fd = true;
  frame.is_extended = true;

  if (driver_->Send(frame) == 0) {
    return true;
  }
  return false;
}

bool CANFDComm::RecvFrame(uint32_t* can_id, uint8_t* data, uint8_t* len,
                          int timeout_ms) {
  if (!driver_) {
    return false;
  }

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

bool CANFDComm::ReadRegisters(uint16_t slave, int addr, int count,
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

bool CANFDComm::WriteRegisters(uint16_t slave, int addr, const std::vector<uint8_t>& data,
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

    uint32_t can_id = PackArbitration(src_id_, slave, 0, func_code, start,
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
      return false;
    }
    if (r_fc == func_code && r_start == 1 && r_end == 1) {
      if (resp_len < 4) return false;
      return resp_data[0] == payload[0] && resp_data[1] == payload[1] &&
             resp_data[2] == payload[2] && resp_data[3] == payload[3];
    }
  }
}

bool CANFDComm::WriteSingleRegister(uint16_t slave, int addr, uint16_t value, int timeout_ms) {
  std::vector<uint8_t> data = {static_cast<uint8_t>((value >> 8) & 0xFF),
                                static_cast<uint8_t>(value & 0xFF)};
  return WriteRegisters(slave, addr, data, timeout_ms);
}

bool CANFDComm::WaitInputResult(uint16_t slave, int addr, int baseline) {
  return WaitCommandMailbox([this, addr, slave](uint16_t* value) {
    std::vector<uint8_t> bytes;
    if (!ReadInputBytes(slave, addr, 1, &bytes) || bytes.size() < 2) return false;
    *value = static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
    return true;
  }, baseline);
}

// Connection management

bool CANFDComm::DetectNode(uint8_t slave_id, int timeout_ms) {
  const bool lite1_reply_is_presence =
      config_.product_type != ProductType::GHand5;
  const uint8_t candidates[] = {slave_id ? slave_id : static_cast<uint8_t>(0x31), 0x32};
  const int count = slave_id ? 1 : 2;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    uint32_t id = 0; uint8_t bytes[64] = {}; uint8_t len = 0;
    if (!RecvFrame(&id, bytes, &len, 50)) continue;
    int src, dst, ack, fc;
    UnpackArbitration(id, &src, &dst, &ack, &fc, nullptr, nullptr, nullptr, nullptr);
    if (ack == 0 && fc == 0x44 && src == dst) {
      for (int i = 0; i < count; ++i) if (src == candidates[i]) return true;
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
      if (fc == 0xC4) {
        if (lite1_reply_is_presence) return true;
        break;
      }
      if (fc == 0x44) {
        return true;
      }
    }
  }
  return false;
}

bool CANFDComm::EstablishConnection(uint8_t slave, int timeout_ms) {
  if (!WriteSingleRegister(slave, 0x0000, 1, timeout_ms)) {
    GHAND_LOG_WARNING("CANFD connect write failed for slave 0x" << std::hex
                      << static_cast<int>(slave) << std::dec);
    return false;
  }
  if (!WaitConnectionState([this, slave](uint16_t* value) {
    std::vector<uint8_t> bytes;
    if (!ReadInputBytes(slave, 0x1047, 1, &bytes) || bytes.size() < 2) return false;
    *value = static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
    return true;
  })) {
    GHAND_LOG_WARNING("CANFD connection state not reached for slave 0x"
                      << std::hex << static_cast<int>(slave) << std::dec);
    return false;
  }
  if (!WaitInputResult(slave, 0x1026)) {
    GHAND_LOG_WARNING("CANFD connect result not successful for slave 0x"
                      << std::hex << static_cast<int>(slave) << std::dec);
    return false;
  }
  return true;
}

bool CANFDComm::EstablishConnectionLite1(uint8_t slave, int timeout_ms) {
  const uint8_t connect_pdu[8] = {0x00, 0x01, 0x00, 0x02,
                                  0x00, 0x00, 0x00, 0x00};
  bool accepted = false;
  for (int attempt = 0; attempt < 2 && !accepted; ++attempt) {
    if (!SendFrame(PackArbitration(src_id_, slave, 0, 0x02), connect_pdu,
                   sizeof(connect_pdu))) {
      return false;
    }

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
      uint32_t resp_id = 0;
      uint8_t resp_data[64] = {};
      uint8_t resp_len = 0;
      const int remaining = static_cast<int>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              deadline - std::chrono::steady_clock::now())
              .count());
      if (!RecvFrame(&resp_id, resp_data, &resp_len, std::max(remaining, 10)))
        continue;

      int r_src = 0, r_dst = 0, r_ack = 0, r_fc = 0;
      UnpackArbitration(resp_id, &r_src, &r_dst, &r_ack, &r_fc,
                        nullptr, nullptr, nullptr, nullptr);
      if (r_ack != 1 || r_dst != src_id_ || r_src != slave) continue;
      if (r_fc == 0x02) {
        accepted = true;
        break;
      }
      if (r_fc == 0x82) {
        if (resp_len >= 1 && resp_data[0] == 0x03) accepted = true;
        break;
      }
    }

    if (!accepted && attempt == 0) {
      // After an aborted handshake the hand can stop answering FC 0x02 until
      // its broadcast reset (FC 0x08) restarts node detection. Reset it and
      // retry once.
      GHAND_LOG_WARNING("CANFD Lite 1 connect unanswered; resetting slave 0x"
                        << std::hex << static_cast<int>(slave) << std::dec);
      const uint8_t reset_pdu[2] = {0x00, 0x01};
      SendFrame(PackArbitration(src_id_, 0x3F, 1, 0x08), reset_pdu,
                sizeof(reset_pdu));
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
  }

  if (!accepted) {
    GHAND_LOG_WARNING("CANFD Lite 1 connect rejected for slave 0x"
                      << std::hex << static_cast<int>(slave) << std::dec);
    return false;
  }
  if (!WaitConnectionState([this, slave](uint16_t* value) {
        std::vector<uint8_t> bytes;
        if (!ReadInputBytes(slave, 0x1047, 1, &bytes) || bytes.size() < 2)
          return false;
        *value = static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
        return true;
      })) {
    GHAND_LOG_WARNING("CANFD Lite 1 connection state not reached for slave 0x"
                      << std::hex << static_cast<int>(slave) << std::dec);
    return false;
  }
  return true;
}

void CANFDComm::DeleteConnection(uint16_t slave) {
  WriteSingleRegister(slave, 0x0000, 2, 100);
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
  GHAND_LOG_INFO("CANFD channel opened: " << device_name << " gear=0x"
                 << std::hex << static_cast<int>(baudrate_gear) << std::dec
                 << " bitrate=" << baudrate_config.bitrate << "/"
                 << baudrate_config.dbitrate);

  connected_.store(true);

  const ProductType product_type = config_.product_type;
  std::vector<uint8_t> detected;
  for (uint8_t id : (slave_id ? std::vector<uint8_t>{slave_id} : std::vector<uint8_t>{0x31, 0x32})) {
    if (DetectNode(id, 500)) detected.push_back(id);
  }
  joint_modes_.clear(); hand_modes_.clear();
  for (uint8_t id : detected) {
    bool established = false;
    if (product_type == ProductType::GHandLite1) {
      established = EstablishConnectionLite1(id);
    } else if (product_type == ProductType::AUTO) {
      established = EstablishConnection(id) || EstablishConnectionLite1(id);
    } else {
      established = EstablishConnection(id);
    }
    if (!established) continue;
    ConnectedHand hand; hand.slave_index = id;
    hand.device_info = GetDeviceInfo(id); hand.hand_type = GetHandType(id);
    { std::lock_guard<std::mutex> lock(hands_mutex_); hands_[id] = hand; }
    if (!keepalive_thread_.joinable()) {
      keepalive_stop_.store(false);
      keepalive_thread_ = std::thread(&CANFDComm::KeepaliveLoop, this);
    }
  }
  if (hands_.empty()) {
    GHAND_LOG_ERROR("Failed to establish CANFD connection");
    driver_->Close();
    connected_.store(false);
    return -3;
  }

  bool should_start_poll = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    should_start_poll = static_cast<bool>(joints_cb_) || static_cast<bool>(hand_state_cb_) || static_cast<bool>(tactile_cb_);
  }
  if (should_start_poll) {
    EnsurePollStarted();
  }
  return 0;
}

int CANFDComm::Disconnect() {
  StopPoll();
  keepalive_stop_.store(true);
  if (keepalive_thread_.joinable()) keepalive_thread_.join();

  if (connected_.load()) {
    for (uint16_t slave : TargetSlaves(0)) DeleteConnection(slave);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  { std::lock_guard<std::mutex> lock(hands_mutex_); hands_.clear(); tactile_open_hands_.clear(); }
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
  StopPoll();
  {
    std::lock_guard<std::mutex> lock(command_mutex_);
    config_ = config;
    joint_modes_.clear(); hand_modes_.clear();
  }
  if (IsConnected()) EnsurePollStarted();
}

bool CANFDComm::SetSlaveId(uint16_t slave, uint8_t new_slave_id) {
  if (!IsConnected() || slave == 0 || new_slave_id == 0 || new_slave_id > 0x3F || TargetSlaves(slave).empty()) return false;
  if (slave == new_slave_id) return true;
  if (!TargetSlaves(new_slave_id).empty()) return false;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  if (!WriteSingleRegister(slave, kSlaveIdRegister, (new_slave_id << 8) | 1)) return false;
  if (!WaitInputResult(new_slave_id, 0x1029)) return false;
  std::lock_guard<std::mutex> hands_lock(hands_mutex_);
  auto hand = hands_[slave]; hands_.erase(slave); hand.slave_index = new_slave_id; hands_[new_slave_id] = hand;
  hand_modes_[new_slave_id] = hand_modes_[slave]; hand_modes_.erase(slave);
  joint_modes_[new_slave_id] = std::move(joint_modes_[slave]); joint_modes_.erase(slave);
  tactile_open_hands_[new_slave_id] = tactile_open_hands_[slave]; tactile_open_hands_.erase(slave);
  return true;
}

bool CANFDComm::SetBaudrateConfig(uint16_t slave, uint8_t baudrate_gear) {
  if (slave == 0 || TargetSlaves(slave).empty()) return false;

  if (!IsConnected()) return false;
  if (baudrate_gear > static_cast<uint8_t>(CANFDBitTiming::TIMING_1M_5M)) {
    return false;
  }
  if (!WriteSingleRegister(slave, kBaudrateConfigRegister,
                           (baudrate_gear << 8) | 1)) {
    return false;
  }
  return WaitInputResult(slave, 0x102A);
}

// Device info

bool CANFDComm::ReadInputBytes(uint16_t slave, int addr, int count,
                               std::vector<uint8_t>* bytes) {
  if (bytes == nullptr) return false;
  return ReadRegisters(slave, addr, count, bytes, 0x04);
}

DeviceInfo CANFDComm::GetDeviceInfo(uint16_t slave) {
  DeviceInfo info;
  if (!IsConnected()) return info;
  std::vector<uint8_t> bytes;
  if (ReadInputBytes(slave, 0x100A, 8, &bytes)) info.device_name = ParseDeviceName(bytes.data(), bytes.size());
  if (ReadInputBytes(slave, 0x1000, 10, &bytes)) info.serial_number = ParseAsciiSerialNumber(bytes.data(), bytes.size());
  auto version = [this, slave](int addr) -> std::string {
    std::vector<uint8_t> bytes;
    if (!ReadInputBytes(slave, addr, 1, &bytes) || bytes.size() < 2) return "N/A";
    return ParsePackedFirmwareVersion(static_cast<uint16_t>((bytes[0] << 8) | bytes[1]));
  };
  const bool lite1 = config_.product_type == ProductType::GHandLite1 ||
                     (config_.product_type == ProductType::AUTO &&
                      info.device_name == "GHand Lite 1");
  if (lite1) {
    info.software_version = version(0x1019);
    info.tactile_sensor_version = version(0x101A);
    info.motor_driver_version = version(0x101B);
    info.firmware_package_version = version(0x101C);
  } else {
    info.software_version = version(0x1019);
    info.position_sensor_version = version(0x101A);
    info.tactile_sensor_version = version(0x101B);
    info.motor_driver_version = version(0x101C);
    info.firmware_package_version = version(0x101D);
  }
  return info;
}

HandType CANFDComm::GetHandType(uint16_t slave) {
  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(slave, 0x1012, 1, &bytes)) {
    GHAND_LOG_ERROR("Failed to read hand type over CANFD");
    return HandType::NONE;
  }
  uint8_t type = ParseHandType(bytes.data(), bytes.size());
  if (type == 1) return HandType::LEFT_HAND;
  if (type == 2) return HandType::RIGHT_HAND;
  return HandType::NONE;
}

// Motion control

bool CANFDComm::MoveJoints(uint16_t slave, const std::vector<JointCommand>& joints, HandControlMode mode) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = MoveJoints(id, joints, mode) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  if (!IsConnected() || joints.empty()) return false;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  auto data = EncodeHandCommand(joints, mode, config_, joint_modes_[slave]);
  if (!WriteRegisters(slave, config_.mode_register, RegistersToBytes(data))) return false;
  hand_modes_[slave] = mode;
  for (const auto& joint : joints) {
    if (!config_.joint_control_registers.count(joint.id)) continue;
    joint_modes_[slave][joint.id] = static_cast<HandControlMode>(joint.control_mode);
  }
  return true;
}

bool CANFDComm::Stop(uint16_t slave) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = Stop(id) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  if (!WriteSingleRegister(slave, config_.mode_register, 0x0100 | static_cast<uint8_t>(hand_modes_[slave]))) return false;
  for (const auto& entry : config_.joint_control_registers) {
    uint16_t value = 0x0200 | static_cast<uint8_t>(joint_modes_[slave][entry.first]);
    if (!WriteSingleRegister(slave, entry.second, value)) return false;
  }
  return true;
}

bool CANFDComm::DisableHand(uint16_t slave) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = DisableHand(id) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  return WriteSingleRegister(slave, config_.mode_register, static_cast<uint8_t>(hand_modes_[slave]));
}

bool CANFDComm::AccessParameters(uint16_t slave, int command_addr,
    int parameter_addr, std::vector<uint16_t>* values, bool write) {
  if (!IsConnected() || slave == 0 || TargetSlaves(slave).empty() ||
      config_.product_type != ProductType::GHand5 || !values || values->empty()) return false;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  const int count = static_cast<int>(values->size());
  // PID command 0x0021 uses input mailbox 0x102B.
  const int mailbox_addr = command_addr + 0x100A;
  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(slave, mailbox_addr, 1, &bytes) || bytes.size() != 2) return false;
  const uint16_t baseline = static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
  if (write) {
    const auto encoded = RegistersToBytes(*values);
    if (!WriteRegisters(slave, parameter_addr, encoded) ||
        !ReadRegisters(slave, parameter_addr, count, &bytes, 0x03) || bytes != encoded) return false;
  }
  if (!WriteSingleRegister(slave, command_addr, write ? 1 : 2) ||
      !WaitInputResult(slave, mailbox_addr, baseline)) return false;
  if (write) return true;
  if (!ReadRegisters(slave, parameter_addr, count, &bytes, 0x03) ||
      bytes.size() != static_cast<size_t>(count * 2)) return false;
  std::vector<uint16_t> readback;
  for (size_t i = 0; i < bytes.size(); i += 2)
    readback.push_back(static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1]));
  *values = std::move(readback);
  return true;
}

bool CANFDComm::GetPidParameters(uint16_t slave, std::vector<uint16_t>* values) {
  if (!values) return false;
  std::vector<uint16_t> parameters(39);
  if (!AccessParameters(slave, 0x0021, 0x0315, &parameters, false)) return false;
  *values = std::move(parameters);
  return true;
}

bool CANFDComm::SetPidParameters(uint16_t slave, const std::vector<uint16_t>& values) {
  if (values.size() != 39) return false;
  auto parameters = values;
  return AccessParameters(slave, 0x0021, 0x0315, &parameters, true);
}

// System operations

bool CANFDComm::ClearFault(uint16_t slave) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = ClearFault(id) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  if (!IsConnected()) return false;
  int previous = -1;
  std::vector<uint8_t> bytes; if (ReadInputBytes(slave, 0x1027, 1, &bytes) && bytes.size() >= 2) previous = (bytes[0] << 8) | bytes[1];
  if (!WriteSingleRegister(slave, kClearFaultRegister, 0x0001)) return false;
  return WaitInputResult(slave, 0x1027, previous);
}

// Tactile sensor

bool CANFDComm::WriteTactileControl(uint16_t slave, uint16_t command) {
  if (!IsConnected()) return false;
  int previous = -1;
  std::vector<uint8_t> bytes; if (ReadInputBytes(slave, 0x1028, 1, &bytes) && bytes.size() >= 2) previous = (bytes[0] << 8) | bytes[1];
  if (!WriteSingleRegister(slave, config_.tactile_control_register, command)) return false;
  return WaitInputResult(slave, 0x1028, previous);
}

bool CANFDComm::OpenTactile(uint16_t slave) {
  if (TargetSlaves(slave).size() != 1 || slave == 0) return false;
  if (!WriteTactileControl(slave, 1)) return false;
  std::lock_guard<std::mutex> lock(hands_mutex_); tactile_open_hands_[slave] = true;
  return true;
}

bool CANFDComm::CloseTactile(uint16_t slave) {
  if (TargetSlaves(slave).size() != 1 || slave == 0) return false;
  if (!WriteTactileControl(slave, 2)) return false;
  std::lock_guard<std::mutex> lock(hands_mutex_); tactile_open_hands_[slave] = false;
  return true;
}

bool CANFDComm::ZeroTactile(uint16_t slave) {
  if (TargetSlaves(slave).size() != 1 || slave == 0) return false;
  if (!WriteTactileControl(slave, 4)) return false;

  return true;
}

// Data retrieval

std::vector<JointData> CANFDComm::GetJoints(uint16_t slave) {
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
  return ParseJoints(regs.data(), regs.size(), config_, start_addr);
}

HandData CANFDComm::GetHandInfo(uint16_t slave, bool* ok) {
  if (ok) *ok = false;
  if (!IsConnected()) return HandData{};

  const bool lite1 = config_.product_type == ProductType::GHandLite1;
  const int register_count = lite1 ? 11 : 14;
  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(slave, 0x1051, register_count, &bytes)) {
    GHAND_LOG_ERROR("Failed to read hand info over CANFD");
    return HandData{};
  }
  std::vector<uint16_t> regs;
  for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
    regs.push_back(static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1]));
  }
  if (ok) *ok = true;
  return lite1 ? ParseHandInfoLite1(regs.data(), regs.size())
               : ParseHandInfo(regs.data(), regs.size());
}

std::map<TactileSensorId, TactileData> CANFDComm::GetTactileData(uint16_t slave) {
  std::map<TactileSensorId, TactileData> data;
  if (!IsConnected() || !config_.has_tactile) return data;

  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(slave, 0x111A, 16, &bytes)) {
    GHAND_LOG_ERROR("Failed to read tactile status over CANFD");
    return data;
  }
  std::vector<uint16_t> regs;
  for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
    regs.push_back(static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1]));
  }

  std::pair<uint8_t, uint8_t> tactile_err = ParseTactileStateError(regs[0]);

  int current_addr = 0x111A + 16;
  for (size_t i = 0; i < config_.tactile_regions.size(); ++i) {
    const auto& region = config_.tactile_regions[i];
    TactileData rt;
    rt.state = (tactile_err.first & (1 << static_cast<int>(i))) != 0;
    rt.resultant_force =
        ParseTactileResultant(regs.data(), static_cast<int>(i));

    int dist_regs = (region.sensor_count * 3 + 1) / 2;
    std::vector<uint8_t> dist_bytes;
    if (!ReadInputBytes(slave, current_addr, dist_regs, &dist_bytes)) {
      GHAND_LOG_ERROR("Failed to read tactile region over CANFD");
      return data;
    }
    rt.distributed_force = ParseTactileDistributed(
        dist_bytes.data(), dist_bytes.size(), region.sensor_count);
    current_addr += dist_regs;

    data.emplace(static_cast<TactileSensorId>(i), std::move(rt));
  }
  return data;
}

// Callbacks

void CANFDComm::SetJointsCallback(JointsCallback cb) {
  bool active = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    joints_cb_ = cb;
    active = static_cast<bool>(joints_cb_) || static_cast<bool>(hand_state_cb_) || static_cast<bool>(tactile_cb_);
  }
  if (active && IsConnected()) EnsurePollStarted();
  else if (!active) StopPoll();
}

void CANFDComm::SetHandStateCallback(HandStateCallback cb) {
  bool active = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    hand_state_cb_ = cb;
    active = static_cast<bool>(joints_cb_) || static_cast<bool>(hand_state_cb_) || static_cast<bool>(tactile_cb_);
  }
  if (active && IsConnected()) EnsurePollStarted();
  else if (!active) StopPoll();
}

void CANFDComm::SetTactileDataCallback(TactileDataCallback cb) {
  bool active = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    tactile_cb_ = cb;
    active = static_cast<bool>(joints_cb_) || static_cast<bool>(hand_state_cb_) || static_cast<bool>(tactile_cb_);
  }
  if (active && IsConnected()) EnsurePollStarted();
  else if (!active) StopPoll();
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
    for (uint16_t slave : TargetSlaves(0)) ReadInputBytes(slave, 0x1047, 1, &bytes);
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
  while (!poll_stop_.load()) {
    JointsCallback joints; HandStateCallback state; TactileDataCallback tactile;
    { std::lock_guard<std::mutex> lock(cb_mutex_); joints = joints_cb_; state = hand_state_cb_; tactile = tactile_cb_; }
    if (IsConnected()) for (const auto& hand : GetConnectedHands()) {
      uint16_t slave = hand.slave_index;
      HandRuntimeState machine = HandRuntimeState::INITIALIZED;
      if (!GetHandStateMachine(slave, &machine)) continue;
      if (machine == HandRuntimeState::POWER_ON_SELF_TEST) {
        if (state) {
          HandData data;
          try { state(slave, data, machine); } catch (...) { }
        }
        continue;
      }
      if (joints && !config_.valid_joints.empty()) {
        auto data = GetJoints(slave);
        if (!data.empty()) try { joints(slave, data); } catch (...) { }
      }
      if (state) {
        bool valid = false; auto data = GetHandInfo(slave, &valid);
        if (valid) try { state(slave, data, machine); } catch (...) { }
      }
      bool enabled = false;
      { std::lock_guard<std::mutex> lock(hands_mutex_); enabled = tactile_open_hands_[slave]; }
      if (enabled && tactile) {
        auto data = GetTactileData(slave);
        if (data.size() == config_.tactile_regions.size())
          try { tactile(slave, data); } catch (...) { }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

bool CANFDComm::GetHandStateMachine(uint16_t slave, HandRuntimeState* state) {
  if (slave == 0 || TargetSlaves(slave).empty()) return false;

  if (!IsConnected() || state == nullptr) return false;
  std::vector<uint8_t> bytes;
  if (!ReadInputBytes(slave, 0x1048, 1, &bytes) || bytes.size() < 2) return false;
  *state = static_cast<HandRuntimeState>(bytes[1]);
  return true;
}

std::vector<ConnectedHand> CANFDComm::GetConnectedHands() {
  std::lock_guard<std::mutex> lock(hands_mutex_);
  std::vector<ConnectedHand> result;
  for (const auto& hand : hands_) result.push_back(hand.second);
  return result;
}
std::vector<uint16_t> CANFDComm::TargetSlaves(uint16_t slave) {
  std::lock_guard<std::mutex> lock(hands_mutex_);
  std::vector<uint16_t> result;
  for (const auto& hand : hands_) if (slave == 0 || slave == hand.first) result.push_back(hand.first);
  return result;
}

}  // namespace internal
}  // namespace ghand
