#include "rs485_comm.h"

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

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
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

bool RS485Comm::SetSlaveId(uint8_t slave_id) {
  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  if (modbus_write_register(ctx_, 0x0000, slave_id) != 1) return false;
  slave_id_ = slave_id;
  GHAND_LOG_INFO("RS485 slave ID set to 0x"
                 << std::hex << static_cast<int>(slave_id) << std::dec);
  return true;
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
  int rc = modbus_read_input_registers(ctx_, 0x1000, 1, &reg);
  if (rc != 1) {
    rc = modbus_read_registers(ctx_, 0x0000, 1, &reg);
  }

  if (rc == 1) {
    slave_id_ = static_cast<uint8_t>(sid);
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

int RS485Comm::Connect(const std::string& device_name) {
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

  const int baud_rates[] = {1000000};
  const int slave_ids[] = {0x32, 0x31};

  for (int baud_rate : baud_rates) {
    ctx_ = modbus_new_rtu(port_name.c_str(), baud_rate, 'N', 8, 1);
    if (!ctx_) {
      const int err = errno;
      GHAND_LOG_ERROR(
          "Failed to create Modbus RTU context: errno="
          << err << ", " << modbus_strerror(err));
      return -1;
    }

    if (modbus_set_response_timeout(ctx_, 0, 500000) == -1) {
      const int err = errno;
      GHAND_LOG_ERROR(
          "Failed to set response timeout: errno="
          << err << ", " << modbus_strerror(err));
    }

    if (modbus_connect(ctx_) == -1) {
      const int err = errno;
      GHAND_LOG_ERROR(
          "modbus_connect failed on "
          << device_name << " at " << baud_rate
          << " baud: errno=" << err << ", " << modbus_strerror(err));
#ifndef _WIN32
      LogLinuxSerialHint(port_name);
#endif
      modbus_free(ctx_);
      ctx_ = nullptr;
      continue;
    }

    GHAND_LOG_INFO(
        "Serial port opened successfully: "
        << device_name << " at " << baud_rate << " baud");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (int attempt = 0; attempt < 1; ++attempt) {
      for (int sid : slave_ids) {
        if (ProbeSlave(sid, attempt, device_name)) {
          return 0;
        }
      }
    }

    CloseContext();
  }

  GHAND_LOG_ERROR("Failed to connect to RS485 device: " << device_name);
  return -2;
}

int RS485Comm::Disconnect() {
  GHAND_LOG_INFO("RS485 disconnecting");

  StopPoll();
  {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    tactile_open_ = false;
    tactile_poll_enabled_ = false;
  }

  CloseContext();

  connected_.store(false);
  slave_id_ = 0x31;
  GHAND_LOG_INFO("RS485 disconnected");
  return 0;
}

// Device info

DeviceInfo RS485Comm::GetDeviceInfo() {
  DeviceInfo info;
  if (!IsConnected()) return info;

  std::vector<uint8_t> bytes;
  if (!ReadInputRegistersBytes(0x1000, 8, &bytes)) {
    GHAND_LOG_ERROR("Failed to read device name");
    return info;
  }
  info.device_name = ParseDeviceName(bytes.data(), bytes.size());

  if (!ReadInputRegistersBytes(0x1008, 8, &bytes)) {
    GHAND_LOG_ERROR("Failed to read hardware version");
    return info;
  }
  info.hardware_version = ParseHardwareVersion(bytes.data(), bytes.size());

  if (!ReadInputRegistersBytes(0x1010, 8, &bytes)) {
    GHAND_LOG_ERROR("Failed to read software version");
    return info;
  }
  info.software_version = ParseFirmwareVersion(bytes.data(), bytes.size());

  if (!ReadInputRegistersBytes(0x1018, 8, &bytes)) {
    GHAND_LOG_ERROR("Failed to read serial number");
    return info;
  }
  info.serial_number = ParseSerialNumber(bytes.data(), bytes.size());

  return info;
}

HandType RS485Comm::GetHandType() {
  if (!IsConnected()) return HandType::NONE;

  std::vector<uint8_t> bytes;
  if (!ReadInputRegistersBytes(0x1020, 1, &bytes)) {
    GHAND_LOG_ERROR("Failed to read hand type");
    return HandType::NONE;
  }
  uint8_t type = ParseHandType(bytes.data(), bytes.size());
  if (type == 1) return HandType::LEFT;
  if (type == 2) return HandType::RIGHT;
  return HandType::NONE;
}

// Motion control

bool RS485Comm::MoveJoints(const std::vector<JointCommand>& joints,
                           ControlMode mode) {
  if (!IsConnected() || joints.empty()) {
    GHAND_LOG_WARNING("MoveJoints called with empty list or not connected");
    return false;
  }

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);

  if (config_.mode_register >= 0) {
    uint16_t mode_value = (static_cast<uint16_t>(mode) << 8) & 0xFF00;
    if (modbus_write_register(ctx_, config_.mode_register, mode_value) != 1) {
      GHAND_LOG_ERROR("Failed to write mode register");
      return false;
    }
  }

  const auto& control_map = config_.joint_control_registers.empty()
                                ? kHoldingRegMap
                                : config_.joint_control_registers;
  for (const auto& joint : joints) {
    auto it = control_map.find(joint.id);
    if (it == control_map.end()) continue;

    auto regs = EncodeJointCommand(joint);
    std::vector<uint16_t> data;
    if (config_.per_joint_mode_control) {
      data.push_back((static_cast<uint16_t>(mode) << 8) & 0xFF00);
    }
    data.push_back(regs.first);
    data.push_back(regs.second);
    int reg_count = static_cast<int>(data.size());
    if (modbus_write_registers(ctx_, it->second, reg_count, data.data()) !=
        reg_count) {
      GHAND_LOG_ERROR("Failed to write joint register: "
                      << ToString(joint.id));
      return false;
    }
  }

  return true;
}

void RS485Comm::Stop() {
  if (!IsConnected()) return;
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  if (config_.stop_register >= 0) {
    modbus_write_register(ctx_, config_.stop_register, 0x0001);
    return;
  }

  const auto& control_map = config_.joint_control_registers.empty()
                                ? kHoldingRegMap
                                : config_.joint_control_registers;
  for (JointId joint_id : config_.valid_joints) {
    auto it = control_map.find(joint_id);
    if (it != control_map.end()) {
      modbus_write_register(ctx_, it->second, 0x0001);
    }
  }
}

// System operations

bool RS485Comm::ClearFault() {
  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  if (modbus_write_register(ctx_, 0x0001, 0x0100) != 1) return false;
  GHAND_LOG_INFO("Fault cleared");
  return true;
}

bool RS485Comm::InitJoint() {
  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  if (modbus_write_register(ctx_, 0x0002, 0x0001) != 1) return false;
  GHAND_LOG_INFO("Joint initialization completed");
  return true;
}

// Tactile sensor

bool RS485Comm::OpenTactile() {
  if (!WriteTactileControl(0x0100)) return false;

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
  bool ok = WriteTactileControl(0x0200);
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

bool RS485Comm::ZeroTactile() { return WriteTactileControl(0x0400); }

bool RS485Comm::WriteTactileControl(uint16_t command) {
  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  return modbus_write_register(ctx_, config_.tactile_control_register,
                               command) == 1;
}

// Data retrieval

bool RS485Comm::ReadInputRegistersBytes(int addr, int count,
                                        std::vector<uint8_t>* bytes) {
  if (!ctx_ || bytes == nullptr) return false;

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  modbus_set_slave(ctx_, slave_id_);
  std::vector<uint16_t> regs(count);
  int rc = modbus_read_input_registers(ctx_, addr, count, regs.data());
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
  if (modbus_read_input_registers(ctx_, start_addr, count, regs.data()) !=
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

  std::lock_guard<std::mutex> io_lock(io_mutex_);
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
    rt.region_name = region.id.c_str();
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

    if (joints_cb) joints_cb(joints);
    if (hand_state_cb) hand_state_cb(hand_state);
    if (tactile_poll_enabled && tactile_cb && has_tactile_data) {
      tactile_cb(tactile_data);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

}  // namespace internal
}  // namespace ghand
