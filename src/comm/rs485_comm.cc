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
inline int modbus_send_raw_request(modbus_t*, const uint8_t*, int) {
  return -1;
}
#else
#include <modbus/modbus.h>
#endif

#include "ghand/logging.h"
#include "logging_macros.h"
#include "modbus_codec.h"
#include "self_test_workflow.h"
#include <map>

namespace ghand {
namespace internal {

static const int kTactilePollIntervalMs = 100;
// A hand that fails this many consecutive polls is skipped for a while so the
// other hand on the bus keeps its polling rate.
static const int kOfflineFailThreshold = 3;
static const int kOfflineBackoffMs = 2000;

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

const int kBaudrateConfigRegister = 0x002C;
const uint8_t kDefaultBaudrateGear = 0x05;

// Modbus function 0x10 accepts at most 123 registers in one frame.
const int kMaxBroadcastRegisters = 123;
// A broadcast write is never acknowledged, so the master must leave the slaves
// time to apply it before the next request (feedback polling) hits the bus.
// Tune on hardware if a hand drops frames right after a broadcast.
const int kBroadcastTurnaroundMs = 3;

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

void RS485Comm::UpdateConfig(const ProductConfig& config) { config_ = config; }

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

int RS485Comm::SendBroadcastRegisters(int addr, int count,
                                      const uint16_t* data) {
  if (ctx_ == nullptr || data == nullptr || count <= 0 ||
      count > kMaxBroadcastRegisters) {
    return -1;
  }

  // Modbus write-multiple-registers PDU with the broadcast address. libmodbus
  // sends the frame and does not wait for a response, which is exactly the
  // broadcast contract.
  std::vector<uint8_t> request;
  request.reserve(static_cast<size_t>(count) * 2 + 7);
  request.push_back(0x00);
  request.push_back(0x10);
  request.push_back(static_cast<uint8_t>((addr >> 8) & 0xFF));
  request.push_back(static_cast<uint8_t>(addr & 0xFF));
  request.push_back(static_cast<uint8_t>((count >> 8) & 0xFF));
  request.push_back(static_cast<uint8_t>(count & 0xFF));
  request.push_back(static_cast<uint8_t>(count * 2));
  for (int i = 0; i < count; ++i) {
    request.push_back(static_cast<uint8_t>((data[i] >> 8) & 0xFF));
    request.push_back(static_cast<uint8_t>(data[i] & 0xFF));
  }

  int rc = modbus_send_raw_request(ctx_, request.data(),
                                   static_cast<int>(request.size()));
  if (rc > 0) {
    tx_packets_++;
  } else {
    tx_errors_++;
  }
  return rc;
}

bool RS485Comm::SetSlaveId(uint16_t slave_index, uint8_t new_slave_id) {
  if (!IsConnected()) return false;
  // A rename always targets exactly one hand: address 0 is the Modbus broadcast
  // address and the Connect() scan sentinel, and renaming every hand at once
  // would collide on the new ID.
  if (slave_index == 0 || slave_index > 0xFF) return false;
  if (new_slave_id == 0) {
    GHAND_LOG_ERROR("Invalid RS485 slave ID: 0x0");
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
      GHAND_LOG_ERROR("RS485 slave 0x" << std::hex
                                       << static_cast<int>(old_id) << std::dec
                                       << " is not on the bus");
      return false;
    }
    if (new_slave_id != old_id) {
      for (const auto& hand : hands_) {
        if (hand.slave_index == new_slave_id) {
          GHAND_LOG_ERROR("RS485 slave ID 0x"
                          << std::hex << static_cast<int>(new_slave_id) << std::dec
                          << " is already used by another hand");
          return false;
        }
      }
    }
  }

  {
    std::lock_guard<std::mutex> io_lock(io_mutex_);
    modbus_set_slave(ctx_, old_id);
    if (WriteRegisterCounted(0x0000, new_slave_id) != 1) return false;
  }

  {
    std::lock_guard<std::mutex> lock(hands_mutex_);
    for (auto& hand : hands_) {
      if (hand.slave_index == old_id) hand.slave_index = new_slave_id;
    }
    health_.erase(old_id);
  }
  GHAND_LOG_INFO("RS485 slave ID set to 0x"
                 << std::hex << static_cast<int>(new_slave_id) << std::dec);
  return true;
}

bool RS485Comm::TargetSlaves(uint16_t slave_index,
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
  GHAND_LOG_ERROR("RS485 slave 0x" << std::hex << static_cast<int>(target)
                                   << std::dec << " is not on the bus");
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
  int rc = ReadInputRegistersCounted(0x1000, 1, &reg);
  if (rc != 1) {
    rc = ReadHoldingRegistersCounted(0x0000, 1, &reg);
  }

  if (rc == 1) {
    GHAND_LOG_INFO(
        "RS485 device found ("
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

  std::vector<uint8_t> found;
  int tried_ids[2] = {0, 0};
  int tried_count = 0;
  for (int i = 0; i < slave_id_count; ++i) {
    int sid = slave_ids[i];
    bool already_tried = false;
    for (int j = 0; j < tried_count; ++j) {
      if (tried_ids[j] == sid) {
        already_tried = true;
        break;
      }
    }
    if (already_tried) continue;
    tried_ids[tried_count++] = sid;
    if (ProbeSlave(sid, 0, device_name)) {
      found.push_back(static_cast<uint8_t>(sid));
    }
  }

  if (found.empty()) {
    CloseContext();
    GHAND_LOG_ERROR("Failed to connect to RS485 device: " << device_name);
    return -2;
  }

  // Enumerate the hands before publishing the connection so the polling
  // thread never observes an empty hand list.
  std::vector<ConnectedHand> hands;
  for (uint8_t target : found) {
    ConnectedHand hand;
    hand.slave_index = target;
    hand.hand_type = GetHandTypeForSlave(target);
    hand.device_info = GetDeviceInfoForSlave(target);
    hands.push_back(std::move(hand));
  }

  {
    std::lock_guard<std::mutex> lock(hands_mutex_);
    hands_ = std::move(hands);
    health_.clear();
  }
  connected_.store(true);
  if (HasCallbacks()) {
    EnsurePollStarted();
  }
  return 0;
}

int RS485Comm::Disconnect() {

  StopPoll();
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    tactile_open_ = false;
    tactile_poll_enabled_ = false;
  }

  CloseContext();

  connected_.store(false);
  {
    std::lock_guard<std::mutex> lock(hands_mutex_);
    hands_.clear();
    health_.clear();
  }
  return 0;
}

std::vector<ConnectedHand> RS485Comm::GetConnectedHands() {
  std::lock_guard<std::mutex> lock(hands_mutex_);
  return hands_;
}

// Device info

DeviceInfo RS485Comm::GetDeviceInfo(uint16_t slave_index) {
  if (!IsConnected() || slave_index == 0 || slave_index > 0xFF) {
    return DeviceInfo{};
  }
  return GetDeviceInfoForSlave(static_cast<uint8_t>(slave_index));
}

DeviceInfo RS485Comm::GetDeviceInfoForSlave(uint8_t slave) {
  DeviceInfo info;

  std::vector<uint8_t> bytes;
  if (ReadInputRegistersBytes(slave, 0x1000, 8, &bytes)) {
    info.device_name = ParseDeviceName(bytes.data(), bytes.size());
    if (info.device_name.empty()) {
      info.device_name = "N/A";
    }
  }

  if (ReadInputRegistersBytes(slave, 0x1008, 8, &bytes)) {
    info.hardware_version = ParseHardwareVersion(bytes.data(), bytes.size());
    if (info.hardware_version.empty()) {
      info.hardware_version = "N/A";
    }
  }

  if (ReadInputRegistersBytes(slave, 0x1010, 8, &bytes)) {
    info.software_version = ParseFirmwareVersion(bytes.data(), bytes.size());
    if (info.software_version.empty()) {
      info.software_version = "N/A";
    }
  }

  if (ReadInputRegistersBytes(slave, 0x119A, 10, &bytes)) {
    std::string serial = ParseAsciiSerialNumber(bytes.data(), bytes.size());
    if (!serial.empty()) {
      info.serial_number = serial;
    }
  }

  if (ReadInputRegistersBytes(slave, 0x1185, 1, &bytes) && bytes.size() >= 2) {
    uint16_t raw = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
    if (raw != 0) info.firmware_package_version = ParsePackedFirmwareVersion(raw);
  }
  if (ReadInputRegistersBytes(slave, 0x1186, 1, &bytes) && bytes.size() >= 2) {
    uint16_t raw = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
    if (raw != 0) info.position_sensor_version = ParsePackedFirmwareVersion(raw);
  }
  if (ReadInputRegistersBytes(slave, 0x1187, 1, &bytes) && bytes.size() >= 2) {
    uint16_t raw = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
    if (raw != 0) info.tactile_sensor_version = ParsePackedFirmwareVersion(raw);
  }
  if (ReadInputRegistersBytes(slave, 0x1188, 1, &bytes) && bytes.size() >= 2) {
    uint16_t raw = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
    if (raw != 0) info.motor_driver_version = ParsePackedFirmwareVersion(raw);
  }
  if (ReadInputRegistersBytes(slave, 0x1189, 1, &bytes) && bytes.size() >= 2) {
    uint16_t raw = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
    if (raw != 0) info.thumb_tactile_sensor_version = ParsePackedFirmwareVersion(raw);
  }
  if (ReadInputRegistersBytes(slave, 0x118A, 1, &bytes) && bytes.size() >= 2) {
    uint16_t raw = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
    if (raw != 0) info.finger_tactile_sensor_version = ParsePackedFirmwareVersion(raw);
  }

  return info;
}

HandType RS485Comm::GetHandType(uint16_t slave_index) {
  if (!IsConnected() || slave_index == 0 || slave_index > 0xFF) {
    return HandType::NONE;
  }
  return GetHandTypeForSlave(static_cast<uint8_t>(slave_index));
}

HandType RS485Comm::GetHandTypeForSlave(uint8_t slave) {
  std::vector<uint8_t> bytes;
  if (!ReadInputRegistersBytes(slave, 0x1020, 1, &bytes)) {
    GHAND_LOG_ERROR("Failed to read hand type");
    return HandType::NONE;
  }
  uint8_t type = ParseHandType(bytes.data(), bytes.size());
  if (type == 1) return HandType::LEFT;
  if (type == 2) return HandType::RIGHT;
  return HandType::NONE;
}

// Motion control

bool RS485Comm::MoveJoints(uint16_t slave_index,
                           const std::vector<JointCommand>& joints,
                           ControlMode mode) {
  if (!IsConnected() || joints.empty()) {
    GHAND_LOG_WARNING("MoveJoints called with empty list or not connected");
    return false;
  }

  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return false;

  uint16_t start_addr = 0;
  std::vector<uint16_t> block;
  if (!BuildJointControlBlock(config_, joints, mode, &start_addr, &block)) {
    GHAND_LOG_ERROR("Failed to encode the joint control block");
    return false;
  }

  const int reg_count = static_cast<int>(block.size());
  std::lock_guard<std::mutex> io_lock(io_mutex_);

  if (targets.size() > 1) {
    // One frame for the whole bus: Modbus broadcast is never acknowledged, so
    // the feedback polling is what confirms the hands actually moved.
    if (SendBroadcastRegisters(start_addr, reg_count, block.data()) <= 0) {
      GHAND_LOG_ERROR("Failed to broadcast the joint control block");
      return false;
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds(kBroadcastTurnaroundMs));
    return true;
  }

  modbus_set_slave(ctx_, targets.front());
  if (WriteRegistersCounted(start_addr, reg_count, block.data()) != reg_count) {
    GHAND_LOG_ERROR("Failed to write the joint control block");
    return false;
  }
  return true;
}

bool RS485Comm::SetBaudrateConfig(uint16_t slave_index,
                                  uint8_t baudrate_gear) {
  if (!IsConnected()) return false;

  int baudrate = 0;
  if (!ResolveRS485Baudrate(baudrate_gear, &baudrate)) {
    GHAND_LOG_ERROR("Invalid RS485 baudrate gear: 0x"
                    << std::hex << static_cast<int>(baudrate_gear)
                    << std::dec);
    return false;
  }

  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return false;

  for (uint8_t slave : targets) {
    std::lock_guard<std::mutex> io_lock(io_mutex_);
    modbus_set_slave(ctx_, slave);
    if (WriteRegisterCounted(kBaudrateConfigRegister, baudrate_gear) != 1) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  GHAND_LOG_INFO("RS485 baudrate config set to gear 0x"
                 << std::hex << static_cast<int>(baudrate_gear) << std::dec
                 << " (" << baudrate
                 << " bps, effective after next power-up)");
  return true;
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

void RS485Comm::Stop(uint16_t slave_index) {
  if (!IsConnected()) return;
  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return;

  for (uint8_t slave : targets) {
    std::lock_guard<std::mutex> io_lock(io_mutex_);
    modbus_set_slave(ctx_, slave);
    if (config_.stop_register >= 0) {
      WriteRegisterCounted(config_.stop_register, 0x0001);
      continue;
    }

    const auto& control_map = config_.joint_control_registers.empty()
                                  ? kHoldingRegMap
                                  : config_.joint_control_registers;
    for (JointId joint_id : config_.valid_joints) {
      auto it = control_map.find(joint_id);
      if (it != control_map.end()) {
        WriteRegisterCounted(it->second, 0x0001);
      }
    }
  }
}

// System operations

bool RS485Comm::ClearFault(uint16_t slave_index) {
  if (!IsConnected()) return false;
  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return false;

  bool ok = true;
  for (uint8_t slave : targets) {
    std::lock_guard<std::mutex> io_lock(io_mutex_);
    modbus_set_slave(ctx_, slave);
    if (WriteRegisterCounted(0x0001, 0x0100) != 1) {
      ok = false;
      continue;
    }
    if (!WaitHoldingResult(slave, 0x0001)) {
      GHAND_LOG_ERROR("Fault clearance failed or timed out on RS485 slave 0x"
                      << std::hex << static_cast<int>(slave) << std::dec);
      ok = false;
    }
  }
  if (ok) GHAND_LOG_INFO("Fault cleared");
  return ok;
}

SelfTestErrorInfo RS485Comm::GetSelfTestErrorInfo(uint16_t slave_index) {
  if (!IsConnected() || slave_index == 0 || slave_index > 0xFF) {
    return SelfTestErrorInfo{};
  }
  const uint8_t slave = static_cast<uint8_t>(slave_index);

  SelfTestRegisterIO io;
  io.read_registers =
      [this, slave](uint16_t address, int count) -> std::vector<uint8_t> {
    std::lock_guard<std::mutex> lock(io_mutex_);
    if (ctx_ == nullptr) return {};
    modbus_set_slave(ctx_, slave);
    std::vector<uint16_t> regs(count);
    if (modbus_read_registers(ctx_, address, count, regs.data()) != count) {
      return {};
    }
    std::vector<uint8_t> bytes;
    bytes.reserve(count * 2);
    for (uint16_t reg : regs) {
      bytes.push_back(static_cast<uint8_t>((reg >> 8) & 0xFF));
      bytes.push_back(static_cast<uint8_t>(reg & 0xFF));
    }
    return bytes;
  };
  io.write_register = [this, slave](uint16_t address, uint16_t value) -> bool {
    std::lock_guard<std::mutex> lock(io_mutex_);
    if (ctx_ == nullptr) return false;
    modbus_set_slave(ctx_, slave);
    return modbus_write_register(ctx_, address, value) == 1;
  };
  return BuildSelfTestErrorInfo(config_, io);
}

uint8_t RS485Comm::GetSelfTestStatus(uint16_t slave_index) {
  if (!IsConnected() || slave_index == 0 || slave_index > 0xFF) return 0;
  std::lock_guard<std::mutex> lock(io_mutex_);
  if (ctx_ == nullptr) return 0;
  modbus_set_slave(ctx_, static_cast<uint8_t>(slave_index));
  uint16_t reg = 0;
  if (modbus_read_registers(ctx_, 0x0038, 1, &reg) != 1) {
    return 0;
  }
  return static_cast<uint8_t>(reg & 0x00FF);
}

// Tactile sensor

bool RS485Comm::OpenTactile(uint16_t slave_index) {
  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return false;

  for (uint8_t slave : targets) {
    if (!WriteTactileControl(slave, 0x0100)) return false;
  }

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

bool RS485Comm::CloseTactile(uint16_t slave_index) {
  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return false;

  bool ok = true;
  for (uint8_t slave : targets) {
    ok = WriteTactileControl(slave, 0x0200) && ok;
  }

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

bool RS485Comm::ZeroTactile(uint16_t slave_index) {
  std::vector<uint8_t> targets;
  if (!TargetSlaves(slave_index, &targets)) return false;

  bool ok = true;
  for (uint8_t slave : targets) {
    ok = WriteTactileControl(slave, 0x0400) && ok;
  }
  return ok;
}

bool RS485Comm::WriteTactileControl(uint8_t slave, uint16_t command) {
  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  return WriteRegisterCounted(config_.tactile_control_register, command) == 1;
}

bool RS485Comm::WaitHoldingResult(uint8_t slave, int addr, int timeout_ms,
                                  int interval_ms) {
  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    uint16_t reg = 0;
    modbus_set_slave(ctx_, slave);
    if (ReadHoldingRegistersCounted(addr, 1, &reg) == 1) {
      uint8_t status = static_cast<uint8_t>(reg & 0x00FF);
      if (status == 1) return true;
      if (status == 2) return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
  }
  timeouts_++;
  return false;
}

// Data retrieval

bool RS485Comm::ReadInputRegistersBytes(uint8_t slave, int addr, int count,
                                        std::vector<uint8_t>* bytes) {
  if (!ctx_ || bytes == nullptr) return false;

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  std::vector<uint16_t> regs(count);
  int rc = ReadInputRegistersCounted(addr, count, regs.data());
  if (rc != count) {
    return false;
  }
  *bytes = RegistersToBytes(regs);
  return true;
}

std::vector<Joint> RS485Comm::GetJoints(uint8_t slave, bool* ok) {
  if (ok != nullptr) *ok = false;
  if (!IsConnected() || config_.valid_joints.empty()) return {};

  uint16_t start_addr = 0;
  int count = 0;
  if (!GetJointInputSpan(config_, &start_addr, &count)) return {};

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  std::vector<uint16_t> regs(count);
  if (ReadInputRegistersCounted(start_addr, count, regs.data()) !=
      count) {
    GHAND_LOG_ERROR("Failed to read joint registers");
    return {};
  }

  if (ok != nullptr) *ok = true;
  return ParseJoints(regs.data(), regs.size(), config_, start_addr);
}

HandState RS485Comm::GetHandInfo(uint8_t slave, bool* ok) {
  if (ok != nullptr) *ok = false;
  if (!IsConnected()) return HandState{};

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  uint16_t regs[2] = {0};
  if (ReadInputRegistersCounted(0x1021, 2, regs) != 2) {
    GHAND_LOG_ERROR("Failed to read hand info registers");
    return HandState{};
  }
  if (ok != nullptr) *ok = true;
  return ParseHandInfo(regs, 2);
}

TactileData RS485Comm::GetTactileData(uint8_t slave, bool* ok) {
  if (ok != nullptr) *ok = false;
  TactileData data;
  if (!IsConnected() || !config_.has_tactile) return data;

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave);
  uint16_t regs[16] = {0};
  if (ReadInputRegistersCounted(0x1080, 16, regs) != 16) {
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

  if (ok != nullptr) *ok = true;
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

    std::vector<ConnectedHand> hands;
    {
      std::lock_guard<std::mutex> lock(hands_mutex_);
      hands = hands_;
    }
    if (hands.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    const bool wants_joints = joints_cb && !config_.valid_joints.empty();
    const bool wants_state = static_cast<bool>(hand_state_cb);
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
            GHAND_LOG_WARNING("RS485 hand 0x"
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
        GHAND_LOG_ERROR("RS485 joints callback failed: " << e.what());
      } catch (...) {
        callback_errors_++;
        GHAND_LOG_ERROR("RS485 joints callback failed with unknown exception");
      }

      try {
        if (wants_state) hand_state_cb(slave, hand_state);
      } catch (const std::exception& e) {
        callback_errors_++;
        GHAND_LOG_ERROR("RS485 hand-state callback failed: " << e.what());
      } catch (...) {
        callback_errors_++;
        GHAND_LOG_ERROR(
            "RS485 hand-state callback failed with unknown exception");
      }

      try {
        if (tactile_due) tactile_cb(slave, tactile_data);
      } catch (const std::exception& e) {
        callback_errors_++;
        GHAND_LOG_ERROR("RS485 tactile callback failed: " << e.what());
      } catch (...) {
        callback_errors_++;
        GHAND_LOG_ERROR("RS485 tactile callback failed with unknown exception");
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

}  // namespace internal
}  // namespace ghand
