#include "rs485_comm.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>

#include "ghand/logging.h"
#include "logging_macros.h"
#include "modbus_codec.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
#include <ntddser.h>
#pragma comment(lib, "setupapi.lib")
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef GHAND_NO_LIBMODBUS
// Stub implementation when libmodbus is not available
struct modbus_t {};
inline modbus_t* modbus_new_rtu(const char*, int, char, int, int) { return nullptr; }
inline void modbus_set_response_timeout(modbus_t*, uint32_t, uint32_t) {}
inline void modbus_set_slave(modbus_t*, int) {}
inline int modbus_connect(modbus_t*) { return -1; }
inline void modbus_close(modbus_t*) {}
inline void modbus_free(modbus_t*) {}
inline int modbus_read_registers(modbus_t*, int, int, uint16_t*) { return -1; }
inline int modbus_read_input_registers(modbus_t*, int, int, uint16_t*) { return -1; }
inline int modbus_write_register(modbus_t*, int, uint16_t) { return -1; }
inline int modbus_write_registers(modbus_t*, int, int, const uint16_t*) { return -1; }
#else
#include <modbus/modbus.h>
#endif

namespace ghand {
namespace internal {

RS485Comm::RS485Comm(const ProductConfig& config) : config_(config) {}

RS485Comm::~RS485Comm() { Disconnect(); }

std::map<std::string, std::string> RS485Comm::SearchAdapters() {
  std::map<std::string, std::string> adapters;

#ifdef _WIN32
  HDEVINFO hDevInfo =
      SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                          DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (hDevInfo != INVALID_HANDLE_VALUE) {
    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) {
      char friendly[256] = {};
      DWORD dataType = 0, reqSize = 0;
      if (SetupDiGetDeviceRegistryPropertyA(
              hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME, &dataType,
              reinterpret_cast<PBYTE>(friendly), sizeof(friendly), &reqSize)) {
        std::string name(friendly);
        size_t lp = name.find('(');
        size_t rp = name.find(')', lp);
        if (lp != std::string::npos && rp != std::string::npos &&
            rp > lp + 1) {
          std::string port = name.substr(lp + 1, rp - lp - 1);
          adapters[port] = name;
        }
      }
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);
  }
#else
  const char* tty_dirs[] = {"/dev/"};
  for (const char* dir : tty_dirs) {
    DIR* d = opendir(dir);
    if (!d) continue;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
      std::string name(entry->d_name);
      if (name.find("ttyUSB") == 0 || name.find("ttyACM") == 0 ||
          name.find("tty.SLAB") == 0 || name.find("tty.wch") == 0) {
        std::string path = std::string(dir) + name;
        adapters[path] = path;
      }
    }
    closedir(d);
  }
#endif

  return adapters;
}

int RS485Comm::Connect(const std::string& device_name) {
  GHAND_LOG_INFO("RS485 connecting to: " << device_name);

  ctx_ = modbus_new_rtu(device_name.c_str(), 1000000, 'N', 8, 1);
  if (!ctx_) {
    GHAND_LOG_ERROR("Failed to create Modbus RTU context");
    return -1;
  }

  modbus_set_response_timeout(ctx_, 0, 500000);  // 500ms

  // Probe slave IDs 0x31 and 0x32
  for (uint8_t sid : {0x31, 0x32}) {
    modbus_set_slave(ctx_, sid);
    if (modbus_connect(ctx_) == 0) {
      uint16_t reg = 0;
      if (modbus_read_registers(ctx_, 0x0000, 1, &reg) == 1) {
        slave_id_ = reg;
        connected_.store(true);
        GHAND_LOG_INFO("RS485 device connected (" << device_name
                                                    << ", slave=0x" << std::hex
                                                    << static_cast<int>(slave_id_)
                                                    << ")");
        return 0;
      }
      modbus_close(ctx_);
    }
  }

  GHAND_LOG_ERROR("Failed to connect to RS485 device: " << device_name);
  modbus_free(ctx_);
  ctx_ = nullptr;
  return -2;
}

int RS485Comm::Disconnect() {
  GHAND_LOG_INFO("RS485 disconnecting");

  StopPoll();

  if (ctx_) {
    modbus_close(ctx_);
    modbus_free(ctx_);
    ctx_ = nullptr;
  }

  connected_.store(false);
  slave_id_ = 0x31;
  GHAND_LOG_INFO("RS485 disconnected");
  return 0;
}

// ===== Device Info =====

DeviceInfo RS485Comm::GetDeviceInfo() {
  DeviceInfo info;
  if (!IsConnected()) return info;

  try {
    auto bytes = ReadInputRegistersBytes(0x1000, 8);
    info.device_name = ParseDeviceName(bytes.data(), bytes.size());

    bytes = ReadInputRegistersBytes(0x1008, 8);
    info.hardware_version = ParseHardwareVersion(bytes.data(), bytes.size());

    bytes = ReadInputRegistersBytes(0x1010, 8);
    info.software_version = ParseFirmwareVersion(bytes.data(), bytes.size());

    bytes = ReadInputRegistersBytes(0x1018, 8);
    info.serial_number = ParseSerialNumber(bytes.data(), bytes.size());
  } catch (...) {
    GHAND_LOG_ERROR("Failed to read device info");
  }

  return info;
}

HandType RS485Comm::GetHandType() {
  if (!IsConnected()) return HandType::NONE;

  try {
    auto bytes = ReadInputRegistersBytes(0x1020, 1);
    uint8_t type = ParseHandType(bytes.data(), bytes.size());
    if (type == 1) return HandType::LEFT;
    if (type == 2) return HandType::RIGHT;
  } catch (...) {
    GHAND_LOG_ERROR("Failed to read hand type");
  }
  return HandType::NONE;
}

// ===== Motion Control =====

bool RS485Comm::MoveJoints(const std::vector<JointCommand>& joints,
                           ControlMode mode) {
  if (!IsConnected() || joints.empty()) {
    GHAND_LOG_WARNING("MoveJoints called with empty list or not connected");
    return false;
  }

  modbus_set_slave(ctx_, slave_id_);

  // Write mode register
  uint16_t mode_value = (static_cast<uint16_t>(mode) << 8) & 0xFF00;
  if (modbus_write_register(ctx_, 0x0010, mode_value) != 1) {
    GHAND_LOG_ERROR("Failed to write mode register");
    return false;
  }

  // Write each joint
  for (const auto& joint : joints) {
    auto it = kHoldingRegMap.find(joint.id);
    if (it == kHoldingRegMap.end()) continue;

    auto regs = EncodeJointCommand(joint);
    uint16_t data[2] = {regs.first, regs.second};
    if (modbus_write_registers(ctx_, it->second, 2, data) != 2) {
      GHAND_LOG_ERROR("Failed to write joint register: " << ToString(joint.id));
      return false;
    }
  }

  return true;
}

void RS485Comm::Stop() {
  if (!IsConnected()) return;
  modbus_set_slave(ctx_, slave_id_);
  modbus_write_register(ctx_, 0x0010, 0x0001);
}

// ===== System Operations =====

bool RS485Comm::ClearFault() {
  if (!IsConnected()) return false;
  modbus_set_slave(ctx_, slave_id_);
  if (modbus_write_register(ctx_, 0x0001, 0x0100) != 1) return false;
  GHAND_LOG_INFO("Fault cleared");
  return true;
}

bool RS485Comm::InitJoint() {
  if (!IsConnected()) return false;
  modbus_set_slave(ctx_, slave_id_);
  if (modbus_write_register(ctx_, 0x0002, 0x0001) != 1) return false;
  GHAND_LOG_INFO("Joint initialization completed");
  return true;
}

// ===== Tactile Sensor =====

bool RS485Comm::OpenTactile() { return WriteTactileControl(0x0100); }

bool RS485Comm::CloseTactile() { return WriteTactileControl(0x0200); }

bool RS485Comm::ZeroTactile() { return WriteTactileControl(0x0400); }

bool RS485Comm::WriteTactileControl(uint16_t command) {
  if (!IsConnected()) return false;
  modbus_set_slave(ctx_, slave_id_);
  return modbus_write_register(ctx_, 0x002B, command) == 1;
}

// ===== Data Retrieval =====

std::vector<uint8_t> RS485Comm::ReadInputRegistersBytes(int addr, int count) {
  if (!ctx_) throw std::runtime_error("Not connected");

  modbus_set_slave(ctx_, slave_id_);
  std::vector<uint16_t> regs(count);
  int rc = modbus_read_input_registers(ctx_, addr, count, regs.data());
  if (rc != count) {
    throw std::runtime_error("Failed to read input registers");
  }
  return RegistersToBytes(regs);
}

std::vector<Joint> RS485Comm::GetJoints() {
  if (!IsConnected() || config_.valid_joints.empty()) return {};

  uint8_t max_id = 0;
  for (auto id : config_.valid_joints) {
    max_id = std::max(max_id, static_cast<uint8_t>(id));
  }
  int count = (max_id + 1) * 3;

  modbus_set_slave(ctx_, slave_id_);
  std::vector<uint16_t> regs(count);
  if (modbus_read_input_registers(ctx_, 0x1023, count, regs.data()) != count) {
    GHAND_LOG_ERROR("Failed to read joint registers");
    return {};
  }

  return ParseJoints(regs.data(), regs.size(), config_.valid_joints);
}

HandState RS485Comm::GetHandInfo() {
  if (!IsConnected()) return HandState{};

  modbus_set_slave(ctx_, slave_id_);
  uint16_t regs[2] = {0};
  if (modbus_read_input_registers(ctx_, 0x1021, 2, regs) != 2) {
    GHAND_LOG_ERROR("Failed to read hand info registers");
    return HandState{};
  }
  return ParseHandInfo(regs, 2);
}

TactileData RS485Comm::GetTactileData() {
  TactileData data;
  if (!IsConnected() || !config_.has_tactile) return data;

  modbus_set_slave(ctx_, slave_id_);
  uint16_t regs[16] = {0};
  if (modbus_read_input_registers(ctx_, 0x1080, 16, regs) != 16) {
    GHAND_LOG_ERROR("Failed to read tactile registers");
    return data;
  }

  std::pair<uint8_t, uint8_t> tactile_err = ParseTactileStateError(regs[0]);
  data.sensor_state = tactile_err.first;
  data.sensor_error = tactile_err.second;

  int current_addr = 0x1080 + 16;
  for (size_t i = 0; i < config_.tactile_regions.size(); ++i) {
    const auto& region = config_.tactile_regions[i];
    int idx = static_cast<int>(i);

    RegionTactile rt;
    rt.region_name = region.name.c_str();
    rt.state = (tactile_err.first & (1 << idx)) != 0;
    rt.resultant_force = ParseTactileResultant(regs, idx);

    int dist_regs = (region.sensor_count * 3 + 1) / 2;
    std::vector<uint16_t> dist_data(dist_regs);
    if (modbus_read_input_registers(ctx_, current_addr, dist_regs,
                                     dist_data.data()) == dist_regs) {
      std::vector<uint8_t> bytes = RegistersToBytes(dist_data);
      rt.distributed_forces =
          ParseTactileDistributed(bytes.data(), bytes.size(),
                                   region.sensor_count);
    }
    current_addr += dist_regs;
    data.regions.push_back(std::move(rt));
  }

  return data;
}

// ===== Callbacks =====

void RS485Comm::SetJointsCallback(JointsCallback cb) {
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

void RS485Comm::SetHandStateCallback(HandStateCallback cb) {
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

void RS485Comm::SetTactileDataCallback(TactileDataCallback cb) {
  std::lock_guard<std::mutex> lock(cb_mutex_);
  tactile_cb_ = cb;
}

// ===== Firmware Update =====

FirmwareUpdateError RS485Comm::BootUpdate(
    const std::string& filename, std::function<void(int)> progress) {
  (void)filename;
  (void)progress;
  GHAND_LOG_WARNING("BootUpdate not supported on RS485");
  return FirmwareUpdateError::NOT_SUPPORTED;
}

bool RS485Comm::QueryFirmwareUpdateResults(uint8_t* main_result,
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

void RS485Comm::EnsurePollStarted() {
  if (!poll_thread_.joinable()) {
    poll_stop_.store(false);
    poll_thread_ = std::thread(&RS485Comm::PollLoop, this);
  }
}

void RS485Comm::StopPoll() {
  poll_stop_.store(true);
  if (poll_thread_.joinable()) {
    poll_thread_.join();
  }
}

void RS485Comm::PollLoop() {
  while (!poll_stop_.load()) {
    if (!connected_.load() || !ctx_) {
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
      GHAND_LOG_ERROR("Poll loop error");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

}  // namespace internal
}  // namespace ghand
