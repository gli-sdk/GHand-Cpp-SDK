// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include "rs485_comm.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#if defined(__MINGW32__) && !defined(_PHYSICAL_ADDRESS)
typedef LARGE_INTEGER PHYSICAL_ADDRESS;
#endif
#include <ntddser.h>
#pragma comment(lib, "setupapi.lib")
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <exception>
#include <sstream>
#include <utility>
#include <string>
#include <vector>

#ifdef GHAND_NO_LIBMODBUS
// Stub implementation when libmodbus is not available
struct modbus_t {};
inline modbus_t* modbus_new_rtu(const char*, int, char, int, int) {
  return nullptr;
}
inline void modbus_set_response_timeout(modbus_t*, uint32_t, uint32_t) {}
inline void modbus_set_slave(modbus_t*, int) {}
inline int modbus_connect(modbus_t*) { return -1; }
inline void modbus_close(modbus_t*) {}
inline void modbus_free(modbus_t*) {}
inline int modbus_read_registers(modbus_t*, int, int, uint16_t*) { return -1; }
inline int modbus_read_input_registers(modbus_t*, int, int, uint16_t*) {
  return -1;
}
inline int modbus_write_register(modbus_t*, int, uint16_t) { return -1; }
inline int modbus_write_registers(modbus_t*, int, int, const uint16_t*) {
  return -1;
}
#else
#include <modbus/modbus.h>
#endif

#include "ghand/logging.h"
#include "logging_macros.h"
#include "modbus_codec.h"
#include <map>

namespace ghand {
namespace internal {

#ifdef _WIN32
namespace {

std::string ByteArrayString(const BYTE* data, size_t size) {
  std::string result;
  for (size_t i = 0; i < size && data[i] != 0; ++i) {
    result.push_back(static_cast<char>(data[i]));
  }
  return result;
}

bool IsLikelyUsbSerialAdapter(const std::string& name) {
  std::string upper_name = name;
  std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  return upper_name.find("USB") != std::string::npos ||
         upper_name.find("CH340") != std::string::npos ||
         upper_name.find("CP210") != std::string::npos ||
         upper_name.find("FTDI") != std::string::npos ||
         upper_name.find("WCH") != std::string::npos;
}

bool ParseWindowsComPort(const std::string& name, int* port_number) {
  if (name.size() <= 3) return false;
  if (std::toupper(static_cast<unsigned char>(name[0])) != 'C' ||
      std::toupper(static_cast<unsigned char>(name[1])) != 'O' ||
      std::toupper(static_cast<unsigned char>(name[2])) != 'M') {
    return false;
  }

  int value = 0;
  for (size_t i = 3; i < name.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(name[i]))) return false;
    value = value * 10 + (name[i] - '0');
  }
  if (port_number) *port_number = value;
  return true;
}

std::string NormalizeWindowsComPort(const std::string& name) {
  int port_number = 0;
  if (ParseWindowsComPort(name, &port_number) && port_number >= 10) {
    return "\\\\.\\" + name;
  }
  return name;
}

}  // namespace
#endif

namespace {

const int kBaudrateConfigRegister = 0x0004;
const int kSlaveIdRegister = 0x0003;
const int kClearFaultRegister = 0x001F;
const uint8_t kDefaultBaudrateGear = 0x05;

bool ResolveRS485Baudrate(uint8_t baudrate_gear, int* baudrate) {
  if (baudrate == nullptr) return false;
  switch (baudrate_gear) {
    case 0x00:
      *baudrate = 57600;
      return true;
    case 0x01:
      *baudrate = 115200;
      return true;
    case 0x02:
      *baudrate = 230400;
      return true;
    case 0x03:
      *baudrate = 460800;
      return true;
    case 0x04:
      *baudrate = 921600;
      return true;
    case 0x05:
      *baudrate = 1000000;
      return true;
    default:
      return false;
  }
}

void AddUniqueAdapter(std::map<std::string, std::string>* adapters,
                      const std::string& path,
                      const std::string& desc) {
  if (path.empty() || adapters->find(path) != adapters->end()) return;
  (*adapters)[path] = desc;
}

#ifndef _WIN32
void AddLinuxSerialById(std::map<std::string, std::string>* adapters) {
  const std::string dir_path = "/dev/serial/by-id";
  DIR* dir = opendir(dir_path.c_str());
  if (!dir) return;

  struct dirent* entry = nullptr;
  while ((entry = readdir(dir)) != nullptr) {
    std::string name(entry->d_name);
    if (name == "." || name == "..") continue;
    AddUniqueAdapter(adapters, dir_path + "/" + name, name);
  }
  closedir(dir);
}

void LogLinuxSerialHint(const std::string& device_name) {
  GHAND_LOG_ERROR("Failed to open RS485 serial port "
                  << device_name
                  << ". On Linux, verify the device exists and the user has "
                     "permission, for example: ls -l "
                  << device_name
                  << "; sudo usermod -aG dialout $USER; then log in again.");
}
#endif

}  // namespace

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
      BYTE friendly[256] = {};
      DWORD dataType = 0, reqSize = 0;
      if (SetupDiGetDeviceRegistryPropertyA(
              hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME, &dataType,
              friendly, sizeof(friendly), &reqSize)) {
        std::string name = ByteArrayString(friendly, sizeof(friendly));
        size_t lp = name.find('(');
        size_t rp = name.find(')', lp);
        if (lp != std::string::npos && rp != std::string::npos &&
            rp > lp + 1) {
          if (!IsLikelyUsbSerialAdapter(name)) continue;
          std::string port = name.substr(lp + 1, rp - lp - 1);
          adapters[port] = name;
        }
      }
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);
  }
#else
  AddLinuxSerialById(&adapters);
  const char* tty_dirs[] = {"/dev/"};
  for (const char* dir : tty_dirs) {
    DIR* d = opendir(dir);
    if (!d) continue;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
      std::string name(entry->d_name);
      if (name.find("ttyUSB") == 0 || name.find("ttyACM") == 0 ||
          name.find("ttyAMA") == 0 || name.find("tty.SLAB") == 0 ||
          name.find("tty.wch") == 0) {
        std::string path = std::string(dir) + name;
        AddUniqueAdapter(&adapters, path, path);
      }
    }
    closedir(d);
  }
#endif

  return adapters;
}

void RS485Comm::UpdateConfig(const ProductConfig& config) {
  std::lock_guard<std::mutex> lock(command_mutex_);
  config_ = config;
  joint_modes_.clear();
  hand_mode_ = ControlMode::POSITION;
}

int RS485Comm::ReadInputRegistersCounted(int addr, int count, uint16_t* dest) {
  int rc = modbus_read_input_registers(ctx_, addr, count, dest);
  if (rc == count) {
    rx_packets_++;
  } else {
    rx_errors_++;
  }
  return rc;
}

int RS485Comm::ReadHoldingRegistersCounted(int addr, int count,
                                           uint16_t* dest) {
  int rc = modbus_read_registers(ctx_, addr, count, dest);
  if (rc == count) {
    rx_packets_++;
  } else {
    rx_errors_++;
  }
  return rc;
}

int RS485Comm::WriteRegisterCounted(int addr, uint16_t value) {
  int rc = modbus_write_register(ctx_, addr, value);
  if (rc == 1) {
    tx_packets_++;
  } else {
    tx_errors_++;
  }
  return rc;
}

int RS485Comm::WriteRegistersCounted(int addr, int count,
                                     const uint16_t* data) {
  int rc = modbus_write_registers(ctx_, addr, count, data);
  if (rc == count) {
    tx_packets_++;
  } else {
    tx_errors_++;
  }
  return rc;
}

bool RS485Comm::SetSlaveId(uint8_t slave_id) {
  if (!IsConnected() || slave_id == 0 || slave_id > 0x3F) return false;
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  if (WriteRegisterCounted(kSlaveIdRegister, (slave_id << 8) | 1) != 1) return false;
  uint8_t old_id = slave_id_;
  slave_id_ = slave_id;
  modbus_set_slave(ctx_, slave_id_);
  if (WaitInputResult(0x1029)) return true;
  slave_id_ = old_id;
  return false;
}

bool RS485Comm::ProbeSlave(int sid, int attempt,
                           const std::string& device_name) {
  GHAND_LOG_INFO(
      "Trying RS485 slave ID 0x"
      << std::hex << sid << std::dec
      << " (attempt " << (attempt + 1) << ")");

  if (modbus_set_slave(ctx_, sid) == -1) {
    const int err = errno;
    GHAND_LOG_ERROR(
        "modbus_set_slave failed for slave 0x"
        << std::hex << sid << std::dec
        << ": errno=" << err
        << ", " << modbus_strerror(err));
    return false;
  }

  uint16_t reg = 0;
  int rc = ReadHoldingRegistersCounted(kSlaveIdRegister, 1, &reg);

  if (rc == 1) {
    slave_id_ = static_cast<uint8_t>(sid);
    if (WriteRegisterCounted(0x0000, 1) != 1 ||
        !WaitConnectionState([this](uint16_t* value) {
          return ReadInputRegistersCounted(0x1047, 1, value) == 1;
        }) || !WaitInputResult(0x1026)) return false;
    joint_modes_.clear();
    hand_mode_ = ControlMode::POSITION;
    connected_.store(true);
    if (HasCallbacks()) {
      EnsurePollStarted();
    }

    GHAND_LOG_INFO(
        "RS485 device connected ("
        << device_name
        << ", probe slave=0x"
        << std::hex << sid
        << ", input register value=0x"
        << reg
        << std::dec << ")");
    return true;
  }

  const int err = errno;
  GHAND_LOG_ERROR(
      "Register probe failed for slave 0x"
      << std::hex << sid << std::dec
      << ": rc=" << rc
      << ", errno=" << err
      << ", " << modbus_strerror(err));
  return false;
}

void RS485Comm::CloseContext() {
  if (ctx_ == nullptr) return;
  modbus_close(ctx_);
  modbus_free(ctx_);
  ctx_ = nullptr;
}

int RS485Comm::Connect(const std::string& device_name, uint8_t slave_id,
                       uint8_t baudrate_gear) {
  if (IsConnected()) return 0;
  GHAND_LOG_INFO("RS485 connecting to: " << device_name);

#ifdef _WIN32
  std::string port_name = NormalizeWindowsComPort(device_name);
#else
  std::string port_name = device_name;
  if (port_name.empty()) {
    std::map<std::string, std::string> adapters = SearchAdapters();
    if (adapters.empty()) {
      GHAND_LOG_ERROR("No RS485 serial adapters found");
      return -1;
    }
    port_name = adapters.begin()->first;
  }
#endif

  const int slave_ids[] = {
      slave_id == 0 ? 0x31 : static_cast<int>(slave_id), 0x32};
  const int slave_id_count = slave_id == 0 ? 2 : 1;
  int baud_rate = 0;
  if (!ResolveRS485Baudrate(baudrate_gear, &baud_rate)) {
    GHAND_LOG_ERROR("Invalid RS485 baudrate gear: 0x"
                    << std::hex << static_cast<int>(baudrate_gear)
                    << std::dec);
    return -1;
  }

  ctx_ = modbus_new_rtu(port_name.c_str(), baud_rate, 'N', 8, 1);
  if (!ctx_) {
    const int err = errno;
    GHAND_LOG_ERROR("Failed to create Modbus RTU context: errno="
                    << err << ", " << modbus_strerror(err));
    return -1;
  }

  if (modbus_set_response_timeout(ctx_, 0, 500000) == -1) {
    const int err = errno;
    GHAND_LOG_ERROR("Failed to set response timeout: errno="
                    << err << ", " << modbus_strerror(err));
  }

  if (modbus_connect(ctx_) == -1) {
    const int err = errno;
    GHAND_LOG_ERROR("modbus_connect failed on "
                    << device_name << " at " << baud_rate
                    << " baud: errno=" << err << ", "
                    << modbus_strerror(err));
#ifndef _WIN32
    LogLinuxSerialHint(port_name);
#endif
    modbus_free(ctx_);
    ctx_ = nullptr;
    return -2;
  }

  GHAND_LOG_INFO("Serial port opened successfully: "
                 << device_name << " at " << baud_rate << " baud");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  for (int attempt = 0; attempt < 1; ++attempt) {
    int tried_ids[2] = {0, 0};
    int tried_count = 0;
    for (int i = 0; i < slave_id_count; ++i) {
      int sid = slave_ids[i];
      bool already_tried = false;
      for (int i = 0; i < tried_count; ++i) {
        if (tried_ids[i] == sid) {
          already_tried = true;
          break;
        }
      }
      if (already_tried) continue;
      tried_ids[tried_count++] = sid;
      if (ProbeSlave(sid, attempt, device_name)) {
        return 0;
      }
    }
  }

  CloseContext();

  GHAND_LOG_ERROR("Failed to connect to RS485 device: " << device_name);
  return -2;
}

int RS485Comm::Disconnect() {

  StopPoll();
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    tactile_open_ = false;
    tactile_poll_enabled_ = false;
  }

  {
    std::lock_guard<std::mutex> io_lock(io_mutex_);
    if (ctx_ && IsConnected()) {
      modbus_set_slave(ctx_, slave_id_);
      WriteRegisterCounted(0x0000, 2);
    }
    CloseContext();
  }

  connected_.store(false);
  slave_id_ = 0x31;
  return 0;
}

// Device info

DeviceInfo RS485Comm::GetDeviceInfo() {
  DeviceInfo info;
  if (!IsConnected()) return info;
  std::vector<uint8_t> bytes;
  if (ReadInputRegistersBytes(0x100A, 8, &bytes)) info.device_name = ParseDeviceName(bytes.data(), bytes.size());
  if (ReadInputRegistersBytes(0x1000, 10, &bytes)) info.serial_number = ParseAsciiSerialNumber(bytes.data(), bytes.size());
  auto version = [this](int addr) -> std::string {
    std::vector<uint8_t> bytes;
    if (!ReadInputRegistersBytes(addr, 1, &bytes) || bytes.size() < 2) return "N/A";
    return ParsePackedFirmwareVersion(static_cast<uint16_t>((bytes[0] << 8) | bytes[1]));
  };
  info.software_version = version(0x1019);
  info.position_sensor_version = version(0x101A);
  info.tactile_sensor_version = version(0x101B);
  info.motor_driver_version = version(0x101C);
  info.firmware_package_version = version(0x101D);
  return info;
}

HandType RS485Comm::GetHandType() {
  if (!IsConnected()) return HandType::NONE;

  std::vector<uint8_t> bytes;
  if (!ReadInputRegistersBytes(0x1012, 1, &bytes)) {
    GHAND_LOG_ERROR("Failed to read hand type");
    return HandType::NONE;
  }
  uint8_t type = ParseHandType(bytes.data(), bytes.size());
  if (type == 1) return HandType::LEFT;
  if (type == 2) return HandType::RIGHT;
  return HandType::NONE;
}

// Motion control

bool RS485Comm::MoveJoints(const std::vector<JointCommand>& joints, ControlMode mode) {
  if (!IsConnected() || joints.empty()) return false;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  auto data = EncodeHandCommand(joints, mode, config_, joint_modes_);
  if (WriteRegistersCounted(config_.mode_register, static_cast<int>(data.size()), data.data()) != static_cast<int>(data.size())) return false;
  hand_mode_ = mode;
  for (const auto& joint : joints) {
    if (!config_.joint_control_registers.count(joint.id)) continue;
    auto previous = joint_modes_.find(joint.id);
    ControlMode fallback = previous == joint_modes_.end() ? ControlMode::POSITION : previous->second;
    joint_modes_[joint.id] = static_cast<ControlMode>(EncodeJointCommand(joint, mode, fallback)[0] & 0x0F);
  }
  return true;
}

bool RS485Comm::SetBaudrateConfig(uint8_t baudrate_gear) {
  if (!IsConnected() || baudrate_gear > 5) return false;
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  if (WriteRegisterCounted(kBaudrateConfigRegister, (baudrate_gear << 8) | 1) != 1) return false;
  return WaitInputResult(0x102A);
}

Diagnostics RS485Comm::GetDiagnostics() const {
  Diagnostics diagnostics;
  diagnostics.connection.connected = IsConnected();
  diagnostics.connection.comm_type = CommType::RS485;
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

void RS485Comm::Stop() {
  if (!IsConnected()) return;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  if (WriteRegisterCounted(config_.mode_register, 0x0100 | static_cast<uint8_t>(hand_mode_)) != 1) return;
  for (const auto& entry : config_.joint_control_registers) {
    uint16_t value = 0x0200 | static_cast<uint8_t>(joint_modes_[entry.first]);
    if (WriteRegisterCounted(entry.second, value) != 1) return;
  }
}

bool RS485Comm::DisableHand() {
  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  return WriteRegisterCounted(config_.mode_register, static_cast<uint8_t>(hand_mode_)) == 1;
}

// System operations

bool RS485Comm::ClearFault() {
  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  uint16_t baseline = 0;
  int previous = -1;
  if (ReadInputRegistersCounted(0x1027, 1, &baseline) == 1) previous = baseline;
  if (WriteRegisterCounted(kClearFaultRegister, 0x0001) != 1) return false;
  return WaitInputResult(0x1027, previous);
}

bool RS485Comm::InitJoint() {
  GHAND_LOG_ERROR("InitJoint is not supported by the new protocol");
  return false;
}

SelfTestErrorInfo RS485Comm::GetSelfTestErrorInfo() {
  GHAND_LOG_ERROR("Legacy self-test commands are not supported by the new protocol");
  return SelfTestErrorInfo{};
}

uint8_t RS485Comm::GetSelfTestStatus() { return 0; }

// Tactile sensor

bool RS485Comm::OpenTactile() {
  if (!WriteTactileControl(0x0001)) return false;

  bool should_start = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    tactile_open_ = true;
    tactile_poll_enabled_ = static_cast<bool>(tactile_cb_);
    should_start = tactile_poll_enabled_;
  }
  if (should_start) {
    EnsurePollStarted();
  }
  return true;
}

bool RS485Comm::CloseTactile() {
  bool ok = WriteTactileControl(0x0002);
  bool should_stop = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    tactile_open_ = false;
    tactile_poll_enabled_ = false;
    should_stop = !joints_cb_ && !hand_state_cb_ && !tactile_poll_enabled_;
  }
  if (should_stop) {
    StopPoll();
  }
  return ok;
}

bool RS485Comm::ZeroTactile() { return WriteTactileControl(0x0004); }

bool RS485Comm::WriteTactileControl(uint16_t command) {
  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  int previous = -1;
  uint16_t baseline = 0; if (ReadInputRegistersCounted(0x1028, 1, &baseline) == 1) previous = baseline;
  if (WriteRegisterCounted(config_.tactile_control_register, command) != 1) return false;
  return WaitInputResult(0x1028, previous);
}

bool RS485Comm::WaitInputResult(int addr, int baseline) {
  return WaitCommandMailbox([this, addr](uint16_t* value) {
    return ReadInputRegistersCounted(addr, 1, value) == 1;
  }, baseline);
}

// Data retrieval

bool RS485Comm::ReadInputRegistersBytes(int addr, int count,
                                        std::vector<uint8_t>* bytes) {
  if (!ctx_ || bytes == nullptr) return false;

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  std::vector<uint16_t> regs(count);
  int rc = ReadInputRegistersCounted(addr, count, regs.data());
  if (rc != count) {
    return false;
  }
  *bytes = RegistersToBytes(regs);
  return true;
}

std::vector<Joint> RS485Comm::GetJoints() {
  if (!IsConnected() || config_.valid_joints.empty()) return {};

  uint16_t start_addr = 0;
  int count = 0;
  if (!GetJointInputSpan(config_, &start_addr, &count)) return {};

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  std::vector<uint16_t> regs(count);
  if (ReadInputRegistersCounted(start_addr, count, regs.data()) !=
      count) {
    GHAND_LOG_ERROR("Failed to read joint registers");
    return {};
  }

  return ParseJoints(regs.data(), regs.size(), config_, start_addr);
}

HandState RS485Comm::GetHandInfo() {
  if (!IsConnected()) return HandState{};

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  uint16_t regs[14] = {0};
  if (ReadInputRegistersCounted(0x1051, 14, regs) != 14) {
    GHAND_LOG_ERROR("Failed to read hand info registers");
    return HandState{};
  }
  return ParseHandInfo(regs, 14);
}

TactileData RS485Comm::GetTactileData() {
  TactileData data;
  if (!IsConnected() || !config_.has_tactile) return data;

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  uint16_t regs[16] = {0};
  if (ReadInputRegistersCounted(0x111A, 16, regs) != 16) {
    GHAND_LOG_ERROR("Failed to read tactile registers");
    return data;
  }

  std::pair<uint8_t, uint8_t> tactile_err = ParseTactileStateError(regs[0]);
  data.sensor_state = tactile_err.first;
  data.sensor_error = tactile_err.second;

  int current_addr = 0x111A + 16;
  for (size_t i = 0; i < config_.tactile_regions.size(); ++i) {
    const auto& region = config_.tactile_regions[i];
    int idx = static_cast<int>(i);

    RegionTactile rt;
    rt.region_name = region.id.c_str();
    rt.state = (tactile_err.first & (1 << idx)) != 0;
    rt.resultant_force = ParseTactileResultant(regs, idx);

    int dist_regs = (region.sensor_count * 3 + 1) / 2;
    std::vector<uint16_t> dist_data(dist_regs);
    if (ReadInputRegistersCounted(current_addr, dist_regs,
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

// Callbacks

void RS485Comm::SetJointsCallback(JointsCallback cb) {
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    joints_cb_ = cb;
  }
  if (cb && IsConnected()) {
    EnsurePollStarted();
  } else if (!cb && !HasCallbacks()) {
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
  } else if (!cb && !HasCallbacks()) {
    StopPoll();
  }
}

void RS485Comm::SetTactileDataCallback(TactileDataCallback cb) {
  bool should_start = false;
  bool should_stop = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    tactile_cb_ = cb;
    tactile_poll_enabled_ = tactile_open_ && static_cast<bool>(cb);
    should_start = tactile_poll_enabled_ && connected_.load();
    should_stop = !joints_cb_ && !hand_state_cb_ && !tactile_poll_enabled_;
  }
  if (should_start) {
    EnsurePollStarted();
  } else if (should_stop) {
    StopPoll();
  }
}

// Firmware update

FirmwareUpdateError RS485Comm::BootUpdate(
    const std::string& filename, std::function<void(int)> progress) {
  (void)filename;
  (void)progress;
  GHAND_LOG_WARNING("BootUpdate not supported on RS485");
  return FirmwareUpdateError::NOT_SUPPORTED;
}

bool RS485Comm::QueryFirmwareUpdateResults(FirmwareUpdateResults* results) {
  (void)results;
  return false;
}

// Polling subscription

bool RS485Comm::HasCallbacks() {
  std::lock_guard<std::mutex> lock(cb_mutex_);
  return static_cast<bool>(joints_cb_) ||
         static_cast<bool>(hand_state_cb_) ||
         (tactile_poll_enabled_ && static_cast<bool>(tactile_cb_));
}

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
  auto last_tactile_time = std::chrono::steady_clock::time_point{};

  while (!poll_stop_.load()) {
    if (!connected_.load() || !ctx_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    JointsCallback joints_cb;
    HandStateCallback hand_state_cb;
    TactileDataCallback tactile_cb;
    bool tactile_poll_enabled = false;
    {
      std::lock_guard<std::mutex> lock(cb_mutex_);
      joints_cb = joints_cb_;
      hand_state_cb = hand_state_cb_;
      tactile_cb = tactile_cb_;
      tactile_poll_enabled = tactile_poll_enabled_;
    }

    if (!joints_cb && !hand_state_cb &&
        (!tactile_poll_enabled || !tactile_cb)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    std::vector<Joint> joints;
    HandState hand_state;
    TactileData tactile_data;
    bool has_tactile_data = false;

    if (joints_cb && !config_.valid_joints.empty()) {
      joints = GetJoints();
    }
    if (hand_state_cb) {
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
      if (joints_cb) joints_cb(joints);
    } catch (const std::exception& e) {
      callback_errors_++;
      GHAND_LOG_ERROR("RS485 joints callback failed: " << e.what());
    } catch (...) {
      callback_errors_++;
      GHAND_LOG_ERROR("RS485 joints callback failed with unknown exception");
    }

    try {
      if (hand_state_cb) hand_state_cb(hand_state);
    } catch (const std::exception& e) {
      callback_errors_++;
      GHAND_LOG_ERROR("RS485 hand-state callback failed: " << e.what());
    } catch (...) {
      callback_errors_++;
      GHAND_LOG_ERROR(
          "RS485 hand-state callback failed with unknown exception");
    }

    try {
      if (tactile_poll_enabled && tactile_cb && has_tactile_data) {
        tactile_cb(tactile_data);
      }
    } catch (const std::exception& e) {
      callback_errors_++;
      GHAND_LOG_ERROR("RS485 tactile callback failed: " << e.what());
    } catch (...) {
      callback_errors_++;
      GHAND_LOG_ERROR("RS485 tactile callback failed with unknown exception");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

bool RS485Comm::GetHandStateMachine(HandStateMachine* state) {
  if (!IsConnected() || state == nullptr) return false;
  std::vector<uint8_t> bytes;
  if (!ReadInputRegistersBytes(0x1048, 1, &bytes) || bytes.size() < 2) return false;
  *state = static_cast<HandStateMachine>(bytes[1]);
  return true;
}

}  // namespace internal
}  // namespace ghand
