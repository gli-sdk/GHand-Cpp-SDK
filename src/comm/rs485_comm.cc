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
  StopPoll();
  {
    std::lock_guard<std::mutex> lock(command_mutex_);
    config_ = config;
    joint_modes_.clear(); hand_modes_.clear();
  }
  if (IsConnected()) EnsurePollStarted();
}

int RS485Comm::ReadInputRegistersCounted(uint16_t slave, int addr, int count, uint16_t* dest) {
  modbus_set_slave(ctx_, slave);
  int rc = modbus_read_input_registers(ctx_, addr, count, dest);
  if (rc == count) {
    rx_packets_++;
  } else {
    rx_errors_++;
  }
  return rc;
}

int RS485Comm::ReadHoldingRegistersCounted(uint16_t slave, int addr, int count,
                                           uint16_t* dest) {
  modbus_set_slave(ctx_, slave);
  int rc = modbus_read_registers(ctx_, addr, count, dest);
  if (rc == count) {
    rx_packets_++;
  } else {
    rx_errors_++;
  }
  return rc;
}

int RS485Comm::WriteRegisterCounted(uint16_t slave, int addr, uint16_t value) {
  modbus_set_slave(ctx_, slave);
  int rc = modbus_write_register(ctx_, addr, value);
  if (rc == 1) {
    tx_packets_++;
  } else {
    tx_errors_++;
  }
  return rc;
}

int RS485Comm::WriteRegistersCounted(uint16_t slave, int addr, int count,
                                     const uint16_t* data) {
  modbus_set_slave(ctx_, slave);
  int rc = modbus_write_registers(ctx_, addr, count, data);
  if (rc == count) {
    tx_packets_++;
  } else {
    tx_errors_++;
  }
  return rc;
}

bool RS485Comm::SetSlaveId(uint16_t slave, uint8_t new_slave_id) {
  if (!IsConnected() || slave == 0 || new_slave_id == 0 || new_slave_id > 0x3F || TargetSlaves(slave).empty()) return false;
  if (slave == new_slave_id) return true;
  if (!TargetSlaves(new_slave_id).empty()) return false;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  if (WriteRegisterCounted(slave, kSlaveIdRegister, (new_slave_id << 8) | 1) != 1) return false;
  if (!WaitInputResult(new_slave_id, 0x1029)) return false;
  std::lock_guard<std::mutex> hands_lock(hands_mutex_);
  auto hand = hands_[slave]; hands_.erase(slave); hand.slave_index = new_slave_id; hands_[new_slave_id] = hand;
  hand_modes_[new_slave_id] = hand_modes_[slave]; hand_modes_.erase(slave);
  joint_modes_[new_slave_id] = std::move(joint_modes_[slave]); joint_modes_.erase(slave);
  tactile_open_hands_[new_slave_id] = tactile_open_hands_[slave]; tactile_open_hands_.erase(slave);
  return true;
}

bool RS485Comm::ProbeLite1Slave(int sid) {
  uint16_t hand_type = 0;
  if (ReadInputRegistersCounted(sid, 0x1012, 1, &hand_type) != 1) return false;
  const uint8_t type = static_cast<uint8_t>(hand_type & 0xFF);
  return type == 1 || type == 2;
}

bool RS485Comm::ProbeSlave(int sid, int attempt, const std::string& device_name) {
  if (config_.product_type == ProductType::GHandLite1) {
    return ProbeLite1Slave(sid);
  }

  uint16_t value = 0;
  if (ReadHoldingRegistersCounted(sid, kSlaveIdRegister, 1, &value) != 1) return false;
  if (WriteRegisterCounted(sid, 0x0000, 1) == 1 &&
      WaitConnectionState([this, sid](uint16_t* value) {
        return ReadInputRegistersCounted(sid, 0x1047, 1, value) == 1;
      }) && WaitInputResult(sid, 0x1026)) {
    return true;
  }
  // Without a product profile the GHand 5 handshake has to be tried first,
  // because a GHand 5 may not answer register reads before it is connected. If
  // the handshake does not complete, the bus may still carry a Lite 1 hand.
  return config_.product_type == ProductType::AUTO && ProbeLite1Slave(sid);
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
        connected_.store(true);
        ConnectedHand hand; hand.slave_index = static_cast<uint16_t>(sid);
        hand.device_info = GetDeviceInfo(sid); hand.hand_type = GetHandType(sid);
        hands_[sid] = hand;
      }
    }
  }

  if (!hands_.empty()) {
    if (HasCallbacks()) EnsurePollStarted();
    return 0;
  }
  CloseContext();

  GHAND_LOG_ERROR("Failed to connect to RS485 device: " << device_name);
  return -2;
}

int RS485Comm::Disconnect() {

  StopPoll();

  {
    std::lock_guard<std::mutex> io_lock(io_mutex_);
    if (ctx_ && IsConnected() &&
        config_.product_type != ProductType::GHandLite1) {
      for (uint16_t slave : TargetSlaves(0)) WriteRegisterCounted(slave, 0x0000, 2);
    }
    CloseContext();
  }

  connected_.store(false);
  { std::lock_guard<std::mutex> lock(hands_mutex_); hands_.clear(); tactile_open_hands_.clear(); }
  joint_modes_.clear(); hand_modes_.clear();
  return 0;
}

// Device info

DeviceInfo RS485Comm::GetDeviceInfo(uint16_t slave) {
  DeviceInfo info;
  if (!IsConnected()) return info;
  std::vector<uint8_t> bytes;
  if (ReadInputRegistersBytes(slave, 0x100A, 8, &bytes)) info.device_name = ParseDeviceName(bytes.data(), bytes.size());
  if (ReadInputRegistersBytes(slave, 0x1000, 10, &bytes)) info.serial_number = ParseAsciiSerialNumber(bytes.data(), bytes.size());
  auto version = [this, slave](int addr) -> std::string {
    std::vector<uint8_t> bytes;
    if (!ReadInputRegistersBytes(slave, addr, 1, &bytes) || bytes.size() < 2) return "N/A";
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

HandType RS485Comm::GetHandType(uint16_t slave) {
  if (!IsConnected()) return HandType::NONE;

  std::vector<uint8_t> bytes;
  if (!ReadInputRegistersBytes(slave, 0x1012, 1, &bytes)) {
    GHAND_LOG_ERROR("Failed to read hand type");
    return HandType::NONE;
  }
  uint8_t type = ParseHandType(bytes.data(), bytes.size());
  if (type == 1) return HandType::LEFT;
  if (type == 2) return HandType::RIGHT;
  return HandType::NONE;
}

// Motion control

bool RS485Comm::MoveJoints(uint16_t slave, const std::vector<JointCommand>& joints, ControlMode mode) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = MoveJoints(id, joints, mode) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  if (!IsConnected() || joints.empty()) return false;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  auto data = EncodeHandCommand(joints, mode, config_, joint_modes_[slave]);
  if (WriteRegistersCounted(slave, config_.mode_register, static_cast<int>(data.size()), data.data()) != static_cast<int>(data.size())) return false;
  hand_modes_[slave] = mode;
  for (const auto& joint : joints) {
    if (!config_.joint_control_registers.count(joint.id)) continue;
    auto previous = joint_modes_[slave].find(joint.id);
    ControlMode fallback = previous == joint_modes_[slave].end() ? ControlMode::POSITION : previous->second;
    joint_modes_[slave][joint.id] = static_cast<ControlMode>(EncodeJointCommand(joint, mode, fallback)[0] & 0x0F);
  }
  return true;
}

bool RS485Comm::SetBaudrateConfig(uint16_t slave, uint8_t baudrate_gear) {
  if (slave == 0 || TargetSlaves(slave).empty()) return false;

  if (!IsConnected() || baudrate_gear > 5) return false;
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  if (WriteRegisterCounted(slave, kBaudrateConfigRegister, (baudrate_gear << 8) | 1) != 1) return false;
  return WaitInputResult(slave, 0x102A);
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

bool RS485Comm::Stop(uint16_t slave) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = Stop(id) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  if (WriteRegisterCounted(slave, config_.mode_register, 0x0100 | static_cast<uint8_t>(hand_modes_[slave])) != 1) return false;
  for (const auto& entry : config_.joint_control_registers) {
    uint16_t value = 0x0200 | static_cast<uint8_t>(joint_modes_[slave][entry.first]);
    if (WriteRegisterCounted(slave, entry.second, value) != 1) return false;
  }
  return true;
}

bool RS485Comm::DisableHand(uint16_t slave) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = DisableHand(id) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> command_lock(command_mutex_);
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  return WriteRegisterCounted(slave, config_.mode_register, static_cast<uint8_t>(hand_modes_[slave])) == 1;
}

// System operations

bool RS485Comm::ClearFault(uint16_t slave) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = ClearFault(id) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  uint16_t baseline = 0;
  int previous = -1;
  if (ReadInputRegistersCounted(slave, 0x1027, 1, &baseline) == 1) previous = baseline;
  if (WriteRegisterCounted(slave, kClearFaultRegister, 0x0001) != 1) return false;
  return WaitInputResult(slave, 0x1027, previous);
}

// Tactile sensor

bool RS485Comm::OpenTactile(uint16_t slave) {
  if (TargetSlaves(slave).size() != 1 || slave == 0) return false;
  if (!WriteTactileControl(slave, 1)) return false;
  std::lock_guard<std::mutex> lock(hands_mutex_); tactile_open_hands_[slave] = true;
  return true;
}

bool RS485Comm::CloseTactile(uint16_t slave) {
  if (TargetSlaves(slave).size() != 1 || slave == 0) return false;
  if (!WriteTactileControl(slave, 2)) return false;
  std::lock_guard<std::mutex> lock(hands_mutex_); tactile_open_hands_[slave] = false;
  return true;
}

bool RS485Comm::ZeroTactile(uint16_t slave) {
  if (TargetSlaves(slave).size() != 1 || slave == 0) return false;
  if (!WriteTactileControl(slave, 4)) return false;

  return true;
}

bool RS485Comm::WriteTactileControl(uint16_t slave, uint16_t command) {
  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  int previous = -1;
  uint16_t baseline = 0; if (ReadInputRegistersCounted(slave, 0x1028, 1, &baseline) == 1) previous = baseline;
  if (WriteRegisterCounted(slave, config_.tactile_control_register, command) != 1) return false;
  return WaitInputResult(slave, 0x1028, previous);
}

bool RS485Comm::WaitInputResult(uint16_t slave, int addr, int baseline) {
  return WaitCommandMailbox([this, addr, slave](uint16_t* value) {
    return ReadInputRegistersCounted(slave, addr, 1, value) == 1;
  }, baseline);
}

// Data retrieval

bool RS485Comm::ReadInputRegistersBytes(uint16_t slave, int addr, int count,
                                        std::vector<uint8_t>* bytes) {
  if (!ctx_ || bytes == nullptr) return false;

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  std::vector<uint16_t> regs(count);
  int rc = ReadInputRegistersCounted(slave, addr, count, regs.data());
  if (rc != count) {
    return false;
  }
  *bytes = RegistersToBytes(regs);
  return true;
}

std::vector<Joint> RS485Comm::GetJoints(uint16_t slave) {
  if (!IsConnected() || config_.valid_joints.empty()) return {};

  uint16_t start_addr = 0;
  int count = 0;
  if (!GetJointInputSpan(config_, &start_addr, &count)) return {};

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  std::vector<uint16_t> regs(count);
  if (ReadInputRegistersCounted(slave, start_addr, count, regs.data()) !=
      count) {
    GHAND_LOG_ERROR("Failed to read joint registers");
    return {};
  }

  return ParseJoints(regs.data(), regs.size(), config_, start_addr);
}

HandState RS485Comm::GetHandInfo(uint16_t slave, bool* ok) {
  if (ok) *ok = false;
  if (!IsConnected()) return HandState{};

  const bool lite1 = config_.product_type == ProductType::GHandLite1;
  const int register_count = lite1 ? 11 : 14;

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  uint16_t regs[14] = {0};
  if (ReadInputRegistersCounted(slave, 0x1051, register_count, regs) !=
      register_count) {
    GHAND_LOG_ERROR("Failed to read hand info registers");
    return HandState{};
  }
  if (ok) *ok = true;
  return lite1 ? ParseHandInfoLite1(regs, register_count)
               : ParseHandInfo(regs, 14);
}

TactileData RS485Comm::GetTactileData(uint16_t slave) {
  TactileData data;
  if (!IsConnected() || !config_.has_tactile) return data;

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  uint16_t regs[16] = {0};
  if (ReadInputRegistersCounted(slave, 0x111A, 16, regs) != 16) {
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
    if (ReadInputRegistersCounted(slave, current_addr, dist_regs,
                                  dist_data.data()) == dist_regs) {
      std::vector<uint8_t> bytes = RegistersToBytes(dist_data);
      rt.distributed_forces =
          ParseTactileDistributed(bytes.data(), bytes.size(),
                                   region.sensor_count);
    } else {
      return TactileData{};
    }
    current_addr += dist_regs;
    data.regions.push_back(std::move(rt));
  }

  return data;
}

// Callbacks

void RS485Comm::SetJointsCallback(JointsCallback cb) {
  bool active = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    joints_cb_ = cb;
    active = static_cast<bool>(joints_cb_) || static_cast<bool>(hand_state_cb_) || static_cast<bool>(tactile_cb_);
  }
  if (active && IsConnected()) EnsurePollStarted();
  else if (!active) StopPoll();
}

void RS485Comm::SetHandStateCallback(HandStateCallback cb) {
  bool active = false;
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    hand_state_cb_ = cb;
    active = static_cast<bool>(joints_cb_) || static_cast<bool>(hand_state_cb_) || static_cast<bool>(tactile_cb_);
  }
  if (active && IsConnected()) EnsurePollStarted();
  else if (!active) StopPoll();
}

void RS485Comm::SetTactileDataCallback(TactileDataCallback cb) {
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
  return static_cast<bool>(joints_cb_) || static_cast<bool>(hand_state_cb_) || static_cast<bool>(tactile_cb_);
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
  while (!poll_stop_.load()) {
    JointsCallback joints; HandStateCallback state; TactileDataCallback tactile;
    { std::lock_guard<std::mutex> lock(cb_mutex_); joints = joints_cb_; state = hand_state_cb_; tactile = tactile_cb_; }
    if (IsConnected()) for (const auto& hand : GetConnectedHands()) {
      uint16_t slave = hand.slave_index;
      if (joints && !config_.valid_joints.empty()) {
        auto data = GetJoints(slave);
        if (!data.empty()) try { joints(slave, data); } catch (...) { callback_errors_++; }
      }
      if (state) {
        bool valid = false; auto data = GetHandInfo(slave, &valid);
        if (valid) try { state(slave, data); } catch (...) { callback_errors_++; }
      }
      bool enabled = false;
      { std::lock_guard<std::mutex> lock(hands_mutex_); enabled = tactile_open_hands_[slave]; }
      if (enabled && tactile) {
        auto data = GetTactileData(slave);
        if (data.regions.size() == config_.tactile_regions.size())
          try { tactile(slave, data); } catch (...) { callback_errors_++; }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

bool RS485Comm::GetHandStateMachine(uint16_t slave, HandStateMachine* state) {
  if (slave == 0 || TargetSlaves(slave).empty()) return false;

  if (!IsConnected() || state == nullptr) return false;
  std::vector<uint8_t> bytes;
  if (!ReadInputRegistersBytes(slave, 0x1048, 1, &bytes) || bytes.size() < 2) return false;
  *state = static_cast<HandStateMachine>(bytes[1]);
  return true;
}

std::vector<ConnectedHand> RS485Comm::GetConnectedHands() {
  std::lock_guard<std::mutex> lock(hands_mutex_);
  std::vector<ConnectedHand> result;
  for (const auto& hand : hands_) result.push_back(hand.second);
  return result;
}
std::vector<uint16_t> RS485Comm::TargetSlaves(uint16_t slave) {
  std::lock_guard<std::mutex> lock(hands_mutex_);
  std::vector<uint16_t> result;
  for (const auto& hand : hands_) if (slave == 0 || slave == hand.first) result.push_back(hand.first);
  return result;
}

}  // namespace internal
}  // namespace ghand
