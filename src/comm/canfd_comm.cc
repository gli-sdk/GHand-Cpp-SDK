#include "canfd_comm.h"

#include <algorithm>
#include <chrono>
#include <cstring>

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

CANFDComm::CANFDComm(const ProductConfig& config)
    : driver_(CreateZLGDriver()), config_(config) {}

CANFDComm::~CANFDComm() { Disconnect(); }

// ===== Arbitration Helpers (Python SDK compatible) =====

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

// ===== Frame I/O =====

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

// ===== Modbus-over-CANFD Transport =====

bool CANFDComm::ReadRegisters(int addr, int count,
                              std::vector<uint8_t>* out_bytes,
                              int func_code, int timeout_ms) {
  // Command frame data: AddrHi, AddrLo, CountHi, CountLo
  uint8_t cmd_data[4];
  cmd_data[0] = (addr >> 8) & 0xFF;
  cmd_data[1] = addr & 0xFF;
  cmd_data[2] = (count >> 8) & 0xFF;
  cmd_data[3] = count & 0xFF;

  uint32_t can_id = PackArbitration(src_id_, dst_id_, 0, func_code, 1, 1, 0, 0);
  if (!SendFrame(can_id, cmd_data, 4)) return false;

  // Gather response segments
  std::map<int, std::vector<uint8_t>> segments;
  auto start_time = std::chrono::steady_clock::now();

  while (true) {
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    int elapsed_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
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
        out_bytes->insert(out_bytes->end(), kv.second.begin(), kv.second.end());
      }
      return true;
    }
  }
}

bool CANFDComm::WriteRegisters(int addr, const std::vector<uint8_t>& data,
                               int timeout_ms) {
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
    int reg_count = data.size() / 2;
    payload.resize(5 + data.size());
    payload[0] = (addr >> 8) & 0xFF;
    payload[1] = addr & 0xFF;
    payload[2] = (reg_count >> 8) & 0xFF;
    payload[3] = reg_count & 0xFF;
    payload[4] = data.size();
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
    int seg_len = std::min(kSegSize, static_cast<int>(payload.size()) - offset);

    uint32_t can_id = PackArbitration(src_id_, dst_id_, 0, func_code, start,
                                       end, toggle, seg_idx);
    if (!SendFrame(can_id, payload.data() + offset, seg_len)) return false;
  }

  // Wait for single-frame response
  auto start_time = std::chrono::steady_clock::now();
  while (true) {
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    int elapsed_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
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

// ===== Connection Management =====

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
  for (uint8_t dst : {0x31, 0x32}) {
    uint8_t data[] = {0x00, 0x31, 0x00, 0x01, 0x00, 0x00};
    uint32_t can_id =
        PackArbitration(src_id_, dst, 0, 0x02, 1, 1, 0, 0);
    SendFrame(can_id, data, sizeof(data));

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
      if (r_fc == 0x82) break;  // Exception, try next dst
      if (r_fc == 0x02) {
        dst_id_ = dst;
        return true;
      }
    }
  }
  return false;
}

void CANFDComm::DeleteConnection() {
  uint8_t data[] = {0x00, 0x30, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00};
  uint32_t can_id = PackArbitration(src_id_, dst_id_, 0, 0x05, 1, 1, 0, 0);
  SendFrame(can_id, data, sizeof(data));
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

  connected_.store(true);

  // Node ID detection (optional, currently skipped as in Python)
  // if (!NodeIdDetection(500)) {
  //   GHAND_LOG_ERROR("Node ID detection timeout");
  //   driver_->Close();
  //   connected_.store(false);
  //   return -3;
  // }

  if (!EstablishConnection(500)) {
    GHAND_LOG_ERROR("Failed to establish CANFD connection");
    driver_->Close();
    connected_.store(false);
    return -3;
  }

  GHAND_LOG_INFO("CANFD device connected (" << device_name << ")");
  return 0;
}

int CANFDComm::Disconnect() {
  GHAND_LOG_INFO("CANFD disconnecting");

  StopPoll();

  if (connected_.load()) {
    DeleteConnection();
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

// ===== Device Info =====

std::vector<uint8_t> CANFDComm::ReadInputBytes(int addr, int count) {
  std::vector<uint8_t> bytes;
  if (!ReadRegisters(addr, count, &bytes, 0x04)) {
    throw std::runtime_error("Failed to read input registers over CANFD");
  }
  return bytes;
}

DeviceInfo CANFDComm::GetDeviceInfo() {
  DeviceInfo info;
  try {
    auto bytes = ReadInputBytes(0x1000, 8);
    info.device_name = ParseDeviceName(bytes.data(), bytes.size());

    bytes = ReadInputBytes(0x1008, 8);
    info.hardware_version = ParseHardwareVersion(bytes.data(), bytes.size());

    bytes = ReadInputBytes(0x1010, 8);
    info.software_version = ParseFirmwareVersion(bytes.data(), bytes.size());

    bytes = ReadInputBytes(0x1018, 8);
    info.serial_number = ParseSerialNumber(bytes.data(), bytes.size());
  } catch (...) {
    GHAND_LOG_ERROR("Failed to read device info over CANFD");
  }
  return info;
}

HandType CANFDComm::GetHandType() {
  try {
    auto bytes = ReadInputBytes(0x1020, 1);
    uint8_t type = ParseHandType(bytes.data(), bytes.size());
    if (type == 1) return HandType::LEFT;
    if (type == 2) return HandType::RIGHT;
  } catch (...) {
    GHAND_LOG_ERROR("Failed to read hand type over CANFD");
  }
  return HandType::NONE;
}

// ===== Motion Control =====

bool CANFDComm::MoveJoints(const std::vector<JointCommand>& joints,
                           ControlMode mode) {
  if (!IsConnected() || joints.empty()) return false;

  // Write mode register
  uint16_t mode_value = (static_cast<uint16_t>(mode) << 8) & 0xFF00;
  if (!WriteSingleRegister(0x0010, mode_value)) return false;

  // Write each joint
  for (const auto& joint : joints) {
    auto it = kHoldingRegMap.find(joint.id);
    if (it == kHoldingRegMap.end()) continue;

    auto regs = EncodeJointCommand(joint);
    std::vector<uint8_t> data = {
        static_cast<uint8_t>((regs.first >> 8) & 0xFF),
        static_cast<uint8_t>(regs.first & 0xFF),
        static_cast<uint8_t>((regs.second >> 8) & 0xFF),
        static_cast<uint8_t>(regs.second & 0xFF)};
    if (!WriteRegisters(it->second, data)) return false;
  }

  return true;
}

void CANFDComm::Stop() {
  if (!IsConnected()) return;
  WriteSingleRegister(0x0010, 0x0001);
}

// ===== System Operations =====

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

// ===== Tactile Sensor =====

bool CANFDComm::WriteTactileControl(uint16_t command) {
  if (!IsConnected()) return false;
  return WriteSingleRegister(0x002B, command);
}

bool CANFDComm::OpenTactile() { return WriteTactileControl(0x0100); }

bool CANFDComm::CloseTactile() { return WriteTactileControl(0x0200); }

bool CANFDComm::ZeroTactile() { return WriteTactileControl(0x0400); }

// ===== Data Retrieval =====

std::vector<Joint> CANFDComm::GetJoints() {
  if (!IsConnected() || config_.valid_joints.empty()) return {};

  uint8_t max_id = 0;
  for (auto id : config_.valid_joints) {
    uint8_t id_val = static_cast<uint8_t>(id);
    if (id_val > max_id) max_id = id_val;
  }
  int count = (max_id + 1) * 3;

  try {
    auto bytes = ReadInputBytes(0x1023, count);
    std::vector<uint16_t> regs;
    regs.reserve(bytes.size() / 2);
    for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
      regs.push_back(static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1]));
    }
    return ParseJoints(regs.data(), regs.size(), config_.valid_joints);
  } catch (...) {
    GHAND_LOG_ERROR("Failed to read joints over CANFD");
    return {};
  }
}

HandState CANFDComm::GetHandInfo() {
  if (!IsConnected()) return HandState{};
  try {
    auto bytes = ReadInputBytes(0x1021, 2);
    std::vector<uint16_t> regs;
    for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
      regs.push_back(static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1]));
    }
    return ParseHandInfo(regs.data(), regs.size());
  } catch (...) {
    GHAND_LOG_ERROR("Failed to read hand info over CANFD");
    return HandState{};
  }
}

TactileData CANFDComm::GetTactileData() {
  TactileData data;
  if (!IsConnected() || !config_.has_tactile) return data;

  try {
    auto bytes = ReadInputBytes(0x1080, 16);
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
      rt.region_name = region.name.c_str();
      rt.state = (tactile_err.first & (1 << static_cast<int>(i))) != 0;
      rt.resultant_force = ParseTactileResultant(regs.data(), static_cast<int>(i));

      int dist_regs = (region.sensor_count * 3 + 1) / 2;
      auto dist_bytes = ReadInputBytes(current_addr, dist_regs);
      rt.distributed_forces = ParseTactileDistributed(
          dist_bytes.data(), dist_bytes.size(), region.sensor_count);
      current_addr += dist_regs;

      data.regions.push_back(std::move(rt));
    }
  } catch (...) {
    GHAND_LOG_ERROR("Failed to read tactile data over CANFD");
  }

  return data;
}

// ===== Callbacks =====

void CANFDComm::SetJointsCallback(JointsCallback cb) {
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    joints_cb_ = cb;
  }
  if (cb && IsConnected()) {
    EnsurePollStarted();
  } else if (!cb && !hand_state_cb_) {
    StopPoll();
  }
}

void CANFDComm::SetHandStateCallback(HandStateCallback cb) {
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    hand_state_cb_ = cb;
  }
  if (cb && IsConnected()) {
    EnsurePollStarted();
  } else if (!cb && !joints_cb_) {
    StopPoll();
  }
}

void CANFDComm::SetTactileDataCallback(TactileDataCallback cb) {
  std::lock_guard<std::mutex> lock(cb_mutex_);
  tactile_cb_ = cb;
}

// ===== Firmware Update =====

FirmwareUpdateError CANFDComm::BootUpdate(const std::string& filename,
                                            std::function<void(int)> progress) {
  (void)filename;
  (void)progress;
  GHAND_LOG_WARNING("BootUpdate not supported on CANFD");
  return FirmwareUpdateError::NOT_SUPPORTED;
}

bool CANFDComm::QueryFirmwareUpdateResults(uint8_t* main_result,
                                            uint8_t* pos_result,
                                            uint8_t* tac_result,
                                            uint8_t* motor_result) {
  (void)main_result;
  (void)pos_result;
  (void)tac_result;
  (void)motor_result;
  return false;
}

// ===== Polling Subscription =====

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

    if (config_.valid_joints.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    try {
      auto joints = GetJoints();
      auto hand_state = GetHandInfo();

      std::lock_guard<std::mutex> lock(cb_mutex_);
      if (joints_cb_) joints_cb_(joints);
      if (hand_state_cb_) hand_state_cb_(hand_state);
    } catch (...) {
      GHAND_LOG_ERROR("CANFD poll loop error");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

}  // namespace internal
}  // namespace ghand
