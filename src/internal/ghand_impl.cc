// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#define _USE_MATH_DEFINES
#include "ghand_impl.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include "modbus_codec.h"
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>
#include <map>
#include <string>

#include "canfd_comm.h"
#include "ghand_callback_manager.h"
#include "ethercat_comm.h"
#include "ghand/logging.h"
#include "logging_macros.h"
#include "product_config_loader.h"
#include "rs485_comm.h"

namespace {

const uint8_t kDefaultSlaveId = 0;
const uint8_t kDefaultBaudrateGear = 0x05;

bool IsVersionNewer(const std::string& old_ver, const std::string& new_ver) {
  auto parse = [](const std::string& ver) {
    std::vector<int> parts;
    std::stringstream ss(ver);
    std::string part;
    while (std::getline(ss, part, '.')) {
      errno = 0;
      char* end = nullptr;
      long value = std::strtol(part.c_str(), &end, 10);
      if (end == part.c_str() || errno == ERANGE ||
          value < std::numeric_limits<int>::min() ||
          value > std::numeric_limits<int>::max()) {
        parts.push_back(0);
      } else {
        parts.push_back(static_cast<int>(value));
      }
    }
    return parts;
  };
  auto old_parts = parse(old_ver);
  auto new_parts = parse(new_ver);
  for (size_t i = 0; i < (std::max)(old_parts.size(), new_parts.size()); ++i) {
    int old_v = i < old_parts.size() ? old_parts[i] : 0;
    int new_v = i < new_parts.size() ? new_parts[i] : 0;
    if (new_v > old_v) return true;
    if (new_v < old_v) return false;
  }
  return false;
}

}  // anonymous namespace

namespace ghand {
namespace internal {

GHandImpl::GHandImpl(ProductType product_type, CommType comm_type)
    : control_mode_(ControlMode::POSITION),
      comm_type_(comm_type),
      product_type_(product_type),
      device_name_(""),
      last_result_(SdkError::OK, "", comm_type) {
  if (product_type == ProductType::AUTO) {
    config_ = ProductConfig();
  } else {
    config_ = LoadProductConfig(product_type);
    if (config_.name.empty()) {
      GHAND_LOG_ERROR("Failed to load product config for product type: "
                << ToString(product_type));
      SetLastResult(SdkError::CONFIG_ERROR, "failed to load product config");
      return;
    }
  }

  switch (comm_type) {
    case CommType::ETHERCAT:
      comm_ = std::unique_ptr<EtherCATComm>(new EtherCATComm(config_));
      break;
    case CommType::CANFD:
      comm_ = std::unique_ptr<CANFDComm>(new CANFDComm(config_));
      break;
    case CommType::RS485:
      comm_ = std::unique_ptr<RS485Comm>(new RS485Comm(config_));
      break;
    default:
      GHAND_LOG_WARNING("Unsupported communication type");
      SetLastResult(SdkError::NOT_SUPPORTED, "unsupported communication type");
      break;
  }
  callback_manager_ =
      std::unique_ptr<GHandCallbackManager>(new GHandCallbackManager());
  if (product_type != ProductType::AUTO) {
    SetupCallbacks();
  }
}

GHandImpl::~GHandImpl() = default;

void GHandImpl::SetLastResult(SdkError code, const std::string& message) {
  std::lock_guard<std::mutex> lock(last_result_mutex_);
  last_result_ = OperationResult(code, message, comm_type_);
}

OperationResult GHandImpl::GetLastResult() const {
  std::lock_guard<std::mutex> lock(last_result_mutex_);
  return last_result_;
}

Diagnostics GHandImpl::GetDiagnostics() const {
  Diagnostics diagnostics = comm_->GetDiagnostics();
  diagnostics.connection.connected = IsConnected();
  diagnostics.connection.comm_type = comm_type_;
  diagnostics.connection.device = device_name_;
  diagnostics.identity.product = product_type_;

  OperationResult last = GetLastResult();
  diagnostics.error.last_sdk_error = last.code;
  diagnostics.error.last_error_message = last.message;
  return diagnostics;
}

void GHandImpl::SetupCallbacks() {
  comm_->SetJointsCallback([this](const std::vector<Joint>& joints) {
    callback_manager_->UpdateJoints(joints);
  });
  comm_->SetHandStateCallback([this](const HandState& state) {
    callback_manager_->UpdateTemperature(state);
  });
  comm_->SetTactileDataCallback([this](const TactileData& data) {
    callback_manager_->UpdateTactileData(data);
  });
}

bool GHandImpl::ConnectToDevice(const std::string& device_name,
                              uint8_t slave_id,
                              uint8_t baudrate_gear) {
  GHAND_LOG_INFO("op=connect comm=" << ToString(comm_type_)
                                    << " device=" << device_name
                                    << " slave_id=0x" << std::hex
                                    << static_cast<int>(slave_id)
                                    << " baudrate_gear=0x"
                                    << static_cast<int>(baudrate_gear)
                                    << std::dec);
  int result = comm_->Connect(device_name, slave_id, baudrate_gear);
  if (result == 0) {
    device_name_ = device_name;
    SetLastResult(SdkError::OK, "");
    GHAND_LOG_INFO("op=connect comm=" << ToString(comm_type_)
                                      << " result=ok device="
                                      << device_name);

    // Post-connection verification / auto-detection
    std::string dev_name = comm_->GetDeviceInfo().device_name;
    if (!dev_name.empty()) {
      if (product_type_ == ProductType::AUTO) {
        config_ = FindConfigByName(dev_name);
        if (config_.name.empty()) {
          GHAND_LOG_ERROR("Auto-detection failed for device: " << dev_name);
          SetLastResult(SdkError::CONFIG_ERROR,
                        "auto-detection failed for device: " + dev_name);
          comm_->Disconnect();
          return false;
        }
        comm_->UpdateConfig(config_);
        SetupCallbacks();
        GHAND_LOG_INFO("Auto-detected product: " << config_.name);
      } else if (!config_.name.empty()) {
        std::vector<std::string> config_names = config_.aliases;
        config_names.push_back(config_.name);
        bool name_matched = false;
        for (const auto& name : config_names) {
          std::string dev_lower = dev_name;
          std::string cfg_lower = name;
          std::transform(dev_lower.begin(), dev_lower.end(), dev_lower.begin(),
                         ::tolower);
          std::transform(cfg_lower.begin(), cfg_lower.end(), cfg_lower.begin(),
                         ::tolower);
          if (dev_lower == cfg_lower) {
            name_matched = true;
            break;
          }
        }
        if (!name_matched) {
          GHAND_LOG_ERROR("Device name mismatch: device reports \""
                      << dev_name << "\", config expects \"" << config_.name
                      << "\"");
          SetLastResult(SdkError::DEVICE_REJECTED,
                        "device name mismatch: " + dev_name);
          comm_->Disconnect();
          return false;
        }
      }
    }
    GHAND_LOG_INFO("op=connect comm=" << ToString(comm_type_)
                                      << " result=ok device_name="
                                      << device_name_);
    return true;
  }
  SetLastResult(SdkError::TRANSPORT_ERROR,
                "connect failed with transport result " +
                    std::to_string(result));
  GHAND_LOG_ERROR("op=connect comm=" << ToString(comm_type_)
                                     << " result=transport_error code="
                                     << result);
  return false;
}

bool GHandImpl::AutoConnect() {
  return AutoConnect(kDefaultSlaveId, kDefaultBaudrateGear);
}

bool GHandImpl::AutoConnect(uint8_t slave_id, uint8_t baudrate_gear) {
  std::map<std::string, std::string> adapter_names = SearchAdapters();
  for (const auto& adapter_pair : adapter_names) {
    if (ConnectToDevice(adapter_pair.first, slave_id, baudrate_gear)) {
      return true;
    }
  }
  return false;
}

bool GHandImpl::Connect(const std::string& device_name, uint8_t slave_id,
                      uint8_t baudrate_gear) {
  if (device_name == "auto") {
    return AutoConnect(slave_id, baudrate_gear);
  }
  return ConnectToDevice(device_name, slave_id, baudrate_gear);
}

bool GHandImpl::Connect(const std::string& device_name, uint8_t slave_id,
                        RS485BaudRate baud_rate) {
  if (comm_type_ != CommType::RS485) {
    GHAND_LOG_ERROR("RS485BaudRate requires RS485 communication");
    SetLastResult(SdkError::INVALID_ARGUMENT,
                  "RS485BaudRate requires RS485 communication");
    return false;
  }
  return Connect(device_name, slave_id, static_cast<uint8_t>(baud_rate));
}

bool GHandImpl::Connect(const std::string& device_name, uint8_t slave_id,
                        CANFDBitTiming timing) {
  if (comm_type_ != CommType::CANFD) {
    GHAND_LOG_ERROR("CANFDBitTiming requires CANFD communication");
    SetLastResult(SdkError::INVALID_ARGUMENT,
                  "CANFDBitTiming requires CANFD communication");
    return false;
  }
  return Connect(device_name, slave_id, static_cast<uint8_t>(timing));
}

bool GHandImpl::Disconnect() {
  GHAND_LOG_INFO("op=disconnect comm=" << ToString(comm_type_));

  Stop();
  int result = comm_->Disconnect();

  if (result == 0) {
    SetLastResult(SdkError::OK, "");
    GHAND_LOG_INFO("op=disconnect comm=" << ToString(comm_type_)
                                         << " result=ok");
  } else {
    SetLastResult(SdkError::TRANSPORT_ERROR, "disconnect failed");
    GHAND_LOG_ERROR("op=disconnect comm=" << ToString(comm_type_)
                                          << " result=transport_error");
  }

  return (result == 0);
}

bool GHandImpl::IsConnected() const { return comm_->IsConnected(); }

bool GHandImpl::SetSlaveId(uint8_t slave_id) {
  if (!IsConnected()) {
    GHAND_LOG_ERROR("Cannot set slave ID: device not connected");
    SetLastResult(SdkError::NOT_CONNECTED, "device is not connected");
    return false;
  }
  bool result = comm_->SetSlaveId(slave_id);
  SetLastResult(result ? SdkError::OK : SdkError::DEVICE_REJECTED,
                result ? "" : "device rejected slave ID change");
  return result;
}

bool GHandImpl::SetBaudrateConfig(RS485BaudRate baud_rate) {
  if (comm_type_ != CommType::RS485) {
    GHAND_LOG_ERROR("RS485BaudRate requires RS485 communication");
    SetLastResult(SdkError::INVALID_ARGUMENT,
                  "RS485BaudRate requires RS485 communication");
    return false;
  }
  if (!IsConnected()) {
    GHAND_LOG_ERROR("Cannot set baudrate config: device not connected");
    SetLastResult(SdkError::NOT_CONNECTED, "device is not connected");
    return false;
  }
  bool result = comm_->SetBaudrateConfig(static_cast<uint8_t>(baud_rate));
  SetLastResult(result ? SdkError::OK : SdkError::DEVICE_REJECTED,
                result ? "" : "device rejected baudrate configuration");
  return result;
}

bool GHandImpl::SetBaudrateConfig(CANFDBitTiming timing) {
  if (comm_type_ != CommType::CANFD) {
    GHAND_LOG_ERROR("CANFDBitTiming requires CANFD communication");
    SetLastResult(SdkError::INVALID_ARGUMENT,
                  "CANFDBitTiming requires CANFD communication");
    return false;
  }
  if (!IsConnected()) {
    GHAND_LOG_ERROR("Cannot set baudrate config: device not connected");
    SetLastResult(SdkError::NOT_CONNECTED, "device is not connected");
    return false;
  }
  bool result = comm_->SetBaudrateConfig(static_cast<uint8_t>(timing));
  SetLastResult(result ? SdkError::OK : SdkError::DEVICE_REJECTED,
                result ? "" : "device rejected baudrate configuration");
  return result;
}

std::map<std::string, std::string> GHandImpl::SearchAdapters() const {
  auto adapters = comm_->SearchAdapters();
  for (const auto& adapter : adapters) {
    GHAND_LOG_DEBUG("Found adapter: " << adapter.first);
  }
  return adapters;
}

HandType GHandImpl::GetHandType() const { return comm_->GetHandType(); }

DeviceInfo GHandImpl::GetDeviceInfo() const { return comm_->GetDeviceInfo(); }

void GHandImpl::SetControlMode(ControlMode mode) { control_mode_ = mode; }

bool GHandImpl::MoveJoints(const std::vector<JointCommand>& joints) {
  // Check device connection status
  if (!IsConnected()) {
    GHAND_LOG_ERROR("Cannot move joints: device not connected");
    SetLastResult(SdkError::NOT_CONNECTED, "device is not connected");
    return false;
  }

  // Check if command is empty
  if (joints.empty()) {
    GHAND_LOG_WARNING("MoveJoints called with empty joint list");
    SetLastResult(SdkError::INVALID_ARGUMENT, "joint command list is empty");
    return false;
  }

  GHAND_LOG_DEBUG("Moving " << joints.size() << " joints");

  // Sort by valid_joints order, include only controllable joints from user
  // input.
  std::vector<JointCommand> ordered_joints;
  ordered_joints.reserve(joints.size());
  for (const auto& joint_id : config_.valid_joints) {
    if (config_.joint_limits.find(joint_id) == config_.joint_limits.end())
      continue;  // Skip read-only joints
    for (auto it = joints.rbegin(); it != joints.rend(); ++it) {
      const auto& joint = *it;
      if (joint.id == joint_id) {
        JointCommand limited = joint;
        uint8_t mode_value = static_cast<uint8_t>(limited.mode);
        if ((limited.mode != ControlMode::INHERIT && (mode_value & 0x0F) > 3) || limited.stop > 2 ||
            (control_mode_ != ControlMode::CUSTOM && static_cast<uint8_t>(control_mode_) > 3) ||
            !std::isfinite(limited.angle)) {
          SetLastResult(SdkError::INVALID_ARGUMENT, "invalid joint mode, state or angle");
          return false;
        }
        ControlMode effective = control_mode_;
        if (effective == ControlMode::CUSTOM && limited.mode != ControlMode::INHERIT)
          effective = static_cast<ControlMode>(mode_value & 0x0F);
        ClampJointAngle(limited);
        int speed = limited.velocity;
        int current = limited.current;
        limited.velocity = static_cast<int16_t>(effective == ControlMode::SPEED
            ? std::max(-100, std::min(100, speed)) : std::min(100, std::abs(speed)));
        limited.current = static_cast<int16_t>(effective == ControlMode::CURRENT
            ? std::max(-100, std::min(100, current)) : std::min(100, std::abs(current)));
        ordered_joints.push_back(limited);
        break;
      }
    }
  }

  if (ordered_joints.empty()) {
    SetLastResult(SdkError::INVALID_ARGUMENT,
                  "joint command list contains no controllable joints");
    return false;
  }

  bool result = comm_->MoveJoints(ordered_joints, control_mode_);
  SetLastResult(result ? SdkError::OK : SdkError::TRANSPORT_ERROR,
                result ? "" : "move joints command failed");
  return result;
}

void GHandImpl::Stop() {
  GHAND_LOG_INFO("op=stop comm=" << ToString(comm_type_));
  comm_->Stop();
}

bool GHandImpl::ClearFault() {
  GHAND_LOG_INFO("op=clear_fault comm=" << ToString(comm_type_));
  bool result = comm_->ClearFault();
  SetLastResult(result ? SdkError::OK : SdkError::DEVICE_REJECTED,
                result ? "" : "clear fault failed");
  GHAND_LOG_INFO("op=clear_fault comm=" << ToString(comm_type_)
                                        << " result="
                                        << (result ? "ok" : "failed"));
  return result;
}

bool GHandImpl::InitJoint() {
  SetLastResult(SdkError::NOT_SUPPORTED, "InitJoint is unavailable in the new protocol");
  return false;
}

bool GHandImpl::OpenTactile() {
  bool result = comm_->OpenTactile();
  SetLastResult(result ? SdkError::OK : SdkError::DEVICE_REJECTED,
                result ? "" : "open tactile failed");
  return result;
}

bool GHandImpl::CloseTactile() {
  bool result = comm_->CloseTactile();
  SetLastResult(result ? SdkError::OK : SdkError::DEVICE_REJECTED,
                result ? "" : "close tactile failed");
  return result;
}

bool GHandImpl::ZeroTactile() {
  bool result = comm_->ZeroTactile();
  SetLastResult(result ? SdkError::OK : SdkError::DEVICE_REJECTED,
                result ? "" : "zero tactile failed");
  return result;
}

void GHandImpl::SetJointsCallback(
    std::function<void(const std::vector<Joint>&)> cb) {
  callback_manager_->SetJointsCallback(cb);
}

void GHandImpl::SetHandStateCallback(std::function<void(const HandState&)> cb) {
  callback_manager_->SetHandStateCallback(cb);
}

void GHandImpl::SetTactileDataCallback(
    std::function<void(const TactileData&)> cb) {
  callback_manager_->SetTactileDataCallback(cb);
}

HandState GHandImpl::GetHandData() const {
  return callback_manager_->GetHandData();
}

std::vector<Joint> GHandImpl::GetJointsData() const {
  return callback_manager_->GetJointsData();
}

TactileData GHandImpl::GetTactileData() const {
  return callback_manager_->GetTactileData();
}

SelfTestErrorInfo GHandImpl::GetSelfTestErrorInfo() {
  SetLastResult(SdkError::NOT_SUPPORTED, "Legacy self-test queries are unavailable; read HandState instead");
  return SelfTestErrorInfo{};
}

uint8_t GHandImpl::GetSelfTestStatus() {
  SetLastResult(SdkError::NOT_SUPPORTED, "Legacy self-test queries are unavailable; read HandState instead");
  return 0;
}

FirmwareUpdateError GHandImpl::BootUpdate(
                        const std::string& filename,
                        std::function<void(int)> progress_callback) {
  GHAND_LOG_INFO("Starting firmware update: " << filename);

  const int retry_count = 10;
  const int retry_delay_ms = 2000;
  std::string last_version = comm_->GetDeviceInfo().software_version;

  FirmwareUpdateError ret =
      comm_->BootUpdate(filename, progress_callback);
  if (ret == FirmwareUpdateError::SUCCESS) {
    for (int i = 0; i < retry_count; i++) {
      progress_callback(100);
      std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));

      if (Connect(device_name_)) {
        FirmwareUpdateResults results;
        if (!comm_->QueryFirmwareUpdateResults(&results)) {
          return FirmwareUpdateError::QUERY_FAILED;
        }

        GHAND_LOG_INFO("Firmware update results - main: "
                       << static_cast<int>(results.main)
                       << ", pos: "
                       << static_cast<int>(results.position_sensor)
                       << ", tactile_mcu: "
                       << static_cast<int>(results.tactile_board)
                       << ", motor: "
                       << static_cast<int>(results.motor_driver)
                       << ", thumb_tactile: "
                       << static_cast<int>(results.thumb_tactile)
                       << ", finger_tactile: "
                       << static_cast<int>(results.finger_tactile));

        if (results.main != 1)
          return FirmwareUpdateError::MAIN_CONTROLLER_FAILED;
        if (results.position_sensor != 1)
          return FirmwareUpdateError::POSITION_SENSOR_FAILED;
        if (results.tactile_board != 1)
          return FirmwareUpdateError::TACTILE_SENSOR_FAILED;
        if (results.motor_driver != 1)
          return FirmwareUpdateError::MOTOR_DRIVER_FAILED;
        if (results.thumb_tactile != 1)
          return FirmwareUpdateError::TACTILE_SENSOR_FAILED;
        if (results.finger_tactile != 1)
          return FirmwareUpdateError::TACTILE_SENSOR_FAILED;

        if (!IsVersionNewer(
                last_version,
                comm_->GetDeviceInfo().software_version)) {
          GHAND_LOG_WARNING("Firmware version not updated after upgrade");
        }

        return FirmwareUpdateError::SUCCESS;
      }
    }
    comm_->Disconnect();
    return FirmwareUpdateError::RECONNECT_FAILED;
  }
  return ret;
}

void GHandImpl::ClampJointAngle(JointCommand& joint) {
  auto it = config_.joint_limits.find(joint.id);
  if (it == config_.joint_limits.end()) {
    return;  // No limit found (e.g., DIP joint)
  }

  const float min_angle = it->second.first;
  const float max_angle = it->second.second;
  if (joint.angle < min_angle) {
    GHAND_LOG_WARNING("[Joint] " << ToString(joint.id) << " angle "
                                 << joint.angle << " < min " << min_angle
                                 << ", set to " << min_angle);
    joint.angle = min_angle;
  } else if (joint.angle > max_angle) {
    GHAND_LOG_WARNING("[Joint] " << ToString(joint.id) << " angle "
                                 << joint.angle << " > max " << max_angle
                                 << ", set to " << max_angle);
    joint.angle = max_angle;
  }
}





bool GHandImpl::DisableHand() {
  if (!IsConnected()) {
    SetLastResult(SdkError::NOT_CONNECTED, "device is not connected");
    return false;
  }
  bool ok = comm_->DisableHand();
  SetLastResult(ok ? SdkError::OK : SdkError::DEVICE_REJECTED,
                ok ? "" : "DisableHand failed");
  return ok;
}

bool GHandImpl::GetHandStateMachine(HandStateMachine* state) {
  if (!IsConnected()) {
    SetLastResult(SdkError::NOT_CONNECTED, "device is not connected");
    return false;
  }
  if (state == nullptr) {
    SetLastResult(SdkError::INVALID_ARGUMENT, "output pointer is null");
    return false;
  }
  bool ok = comm_->GetHandStateMachine(state);
  SetLastResult(ok ? SdkError::OK : SdkError::DEVICE_REJECTED,
                ok ? "" : "GetHandStateMachine failed");
  return ok;
}

bool GHandImpl::GetStallTolerances(std::map<JointId, uint8_t>* values) {
  if (!IsConnected()) {
    SetLastResult(SdkError::NOT_CONNECTED, "device is not connected");
    return false;
  }
  if (comm_type_ != CommType::ETHERCAT) {
    SetLastResult(SdkError::NOT_SUPPORTED, "parameter access requires EtherCAT");
    return false;
  }
  if (values == nullptr) {
    SetLastResult(SdkError::INVALID_ARGUMENT, "output pointer is null");
    return false;
  }
  bool ok = comm_->GetStallTolerances(values);
  SetLastResult(ok ? SdkError::OK : SdkError::DEVICE_REJECTED,
                ok ? "" : "GetStallTolerances failed");
  return ok;
}

bool GHandImpl::SetStallTolerances(const std::map<JointId, uint8_t>& values) {
  if (!IsConnected()) {
    SetLastResult(SdkError::NOT_CONNECTED, "device is not connected");
    return false;
  }
  if (comm_type_ != CommType::ETHERCAT) {
    SetLastResult(SdkError::NOT_SUPPORTED, "parameter access requires EtherCAT");
    return false;
  }
  if (values.size() != 13) {
    SetLastResult(SdkError::INVALID_ARGUMENT, "stall tolerances require all 13 active joints");
    return false;
  }
  bool ok = comm_->SetStallTolerances(values);
  SetLastResult(ok ? SdkError::OK : SdkError::DEVICE_REJECTED,
                ok ? "" : "SetStallTolerances failed");
  return ok;
}

bool GHandImpl::GetPidParameters(std::vector<uint16_t>* values) {
  if (!IsConnected()) {
    SetLastResult(SdkError::NOT_CONNECTED, "device is not connected");
    return false;
  }
  if (comm_type_ != CommType::ETHERCAT) {
    SetLastResult(SdkError::NOT_SUPPORTED, "parameter access requires EtherCAT");
    return false;
  }
  if (values == nullptr) {
    SetLastResult(SdkError::INVALID_ARGUMENT, "output pointer is null");
    return false;
  }
  bool ok = comm_->GetPidParameters(values);
  SetLastResult(ok ? SdkError::OK : SdkError::DEVICE_REJECTED,
                ok ? "" : "GetPidParameters failed");
  return ok;
}

bool GHandImpl::SetPidParameters(const std::vector<uint16_t>& values) {
  if (!IsConnected()) {
    SetLastResult(SdkError::NOT_CONNECTED, "device is not connected");
    return false;
  }
  if (comm_type_ != CommType::ETHERCAT) {
    SetLastResult(SdkError::NOT_SUPPORTED, "parameter access requires EtherCAT");
    return false;
  }
  if (values.size() != 39) {
    SetLastResult(SdkError::INVALID_ARGUMENT, "PID parameters require 39 uint16 values");
    return false;
  }
  bool ok = comm_->SetPidParameters(values);
  SetLastResult(ok ? SdkError::OK : SdkError::DEVICE_REJECTED,
                ok ? "" : "SetPidParameters failed");
  return ok;
}

}  // namespace internal
}  // namespace ghand
