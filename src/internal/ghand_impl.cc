// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#define _USE_MATH_DEFINES
#include "ghand_impl.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
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

// Check per-MCU firmware upgrade results. Returns SUCCESS only when every
// MCU reports success (result == 1); otherwise the first failing MCU maps
// to its corresponding FirmwareUpdateError.
ghand::FirmwareUpdateError CheckFirmwareUpdateResults(
    const ghand::internal::FirmwareUpdateResults& results) {
  if (results.main == 0)
    return ghand::FirmwareUpdateError::MAIN_CONTROLLER_FAILED;
  if (results.position_sensor == 0)
    return ghand::FirmwareUpdateError::POSITION_SENSOR_FAILED;
  if (results.tactile_board == 0)
    return ghand::FirmwareUpdateError::TACTILE_SENSOR_FAILED;
  if (results.motor_driver == 0)
    return ghand::FirmwareUpdateError::MOTOR_DRIVER_FAILED;
  if (results.thumb_tactile == 0 || results.finger_tactile == 0)
    return ghand::FirmwareUpdateError::TACTILE_SENSOR_FAILED;
  return ghand::FirmwareUpdateError::SUCCESS;
}

}  // anonymous namespace

namespace ghand {
namespace internal {

GHandImpl::GHandImpl(ProductType product_type, CommType comm_type)
    : comm_type_(comm_type),
      product_type_(product_type),
      device_name_("") {
  if (product_type == ProductType::AUTO) {
    config_ = ProductConfig();
  } else {
    config_ = LoadProductConfig(product_type);
    if (config_.name.empty()) {
      GHAND_LOG_ERROR("Failed to load product config for product type: "
                << ToString(product_type));
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
      break;
  }
  callback_manager_ =
      std::unique_ptr<GHandCallbackManager>(new GHandCallbackManager());
  if (product_type != ProductType::AUTO) {
    SetupCallbacks();
  }
}

GHandImpl::~GHandImpl() = default;

Diagnostics GHandImpl::GetDiagnostics() const {
  Diagnostics diagnostics = comm_->GetDiagnostics();
  diagnostics.connection.connected = IsConnected();
  diagnostics.connection.comm_type = comm_type_;
  diagnostics.connection.device = device_name_;
  diagnostics.identity.product = product_type_;

  return diagnostics;
}

void GHandImpl::SetupCallbacks() {
  comm_->SetJointsCallback([this](uint16_t slave,
                                  const std::vector<Joint>& joints) {
    callback_manager_->UpdateJoints(slave, joints);
  });
  comm_->SetHandStateCallback([this](uint16_t slave, const HandState& state) {
    callback_manager_->UpdateTemperature(slave, state);
  });
  comm_->SetTactileDataCallback([this](uint16_t slave,
                                       const TactileData& data) {
    callback_manager_->UpdateTactileData(slave, data);
  });
}

ResultOf<std::vector<ConnectedHand>> GHandImpl::ConnectToDevice(
    const std::string& device_name, uint8_t slave_id, uint8_t baudrate_gear) {
  device_info_ = DeviceInfo();
  int result = comm_->Connect(device_name, slave_id, baudrate_gear);
  if (result == 0) {
    device_name_ = device_name;
    GHAND_LOG_INFO("comm=" << ToString(comm_type_)
                                      << " result=ok device="
                                      << device_name);

    const std::vector<ConnectedHand> hands = comm_->GetConnectedHands();
    if (hands.empty()) {
      comm_->Disconnect();
      return {SdkError::DEVICE_REJECTED,
              "no controllable hand found on the bus"};
    }

    // Post-connection verification / auto-detection. The first hand on the
    // bus identifies the product.
    device_info_ = comm_->GetDeviceInfo(hands.front().slave_index);
    std::string dev_name = device_info_.device_name;
    if (!dev_name.empty()) {
      if (product_type_ == ProductType::AUTO) {
        config_ = FindConfigByName(dev_name);
        if (config_.name.empty()) {
          GHAND_LOG_ERROR("Auto-detection failed for device: " << dev_name);
          comm_->Disconnect();
          return {SdkError::CONFIG_ERROR,
                  "auto-detection failed for device: " + dev_name};
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
          comm_->Disconnect();
          return {SdkError::PRODUCT_MISMATCH,
                  "device name mismatch: " + dev_name};
        }
      }
    }
    GHAND_LOG_INFO("op=connect comm=" << ToString(comm_type_)
                                      << " result=ok device_name="
                                      << device_name_);
    return hands;
  }
  GHAND_LOG_ERROR("op=connect comm=" << ToString(comm_type_)
                                     << " result=transport_error code="
                                     << result);
  return {SdkError::TRANSPORT_ERROR,
          "connect failed with transport result " + std::to_string(result)};
}

ResultOf<std::vector<ConnectedHand>> GHandImpl::AutoConnect(
    uint8_t slave_id, uint8_t baudrate_gear) {
  device_info_ = DeviceInfo();
  ResultOf<std::vector<ConnectedHand>> mismatch_result;
  DeviceInfo mismatch_info;
  std::string mismatch_device;
  std::map<std::string, std::string> adapter_names = SearchAdapters();
  if (adapter_names.empty()) {
    return {SdkError::NOT_CONNECTED, "no communication adapters found"};
  }
  ResultOf<std::vector<ConnectedHand>> result{
      SdkError::TRANSPORT_ERROR, "no adapter carries a controllable hand"};
  for (const auto& adapter_pair : adapter_names) {
    result = ConnectToDevice(adapter_pair.first, slave_id, baudrate_gear);
    if (result) return result;
    if (result.code == SdkError::PRODUCT_MISMATCH &&
        mismatch_result.code != SdkError::PRODUCT_MISMATCH) {
      mismatch_result = result;
      mismatch_info = device_info_;
      mismatch_device = adapter_pair.first;
    }
  }
  if (mismatch_result.code == SdkError::PRODUCT_MISMATCH) {
    device_info_ = mismatch_info;
    device_name_ = mismatch_device;
    return mismatch_result;
  }
  return result;
}

ResultOf<std::vector<ConnectedHand>> GHandImpl::Connect(
    const std::string& device_name, uint8_t slave_id, uint8_t baudrate_gear) {
  if (device_name == "auto") {
    return AutoConnect(slave_id, baudrate_gear);
  }
  return ConnectToDevice(device_name, slave_id, baudrate_gear);
}

ResultOf<std::vector<ConnectedHand>> GHandImpl::Connect(
    const std::string& device_name, uint8_t slave_id, RS485BaudRate baud_rate) {
  if (comm_type_ != CommType::RS485) {
    device_info_ = DeviceInfo();
    GHAND_LOG_ERROR("RS485BaudRate requires RS485 communication");
    return {SdkError::INVALID_ARGUMENT,
            "RS485BaudRate requires RS485 communication"};
  }
  return Connect(device_name, slave_id, static_cast<uint8_t>(baud_rate));
}

ResultOf<std::vector<ConnectedHand>> GHandImpl::Connect(
    const std::string& device_name, uint8_t slave_id, CANFDBitTiming timing) {
  if (comm_type_ != CommType::CANFD) {
    device_info_ = DeviceInfo();
    GHAND_LOG_ERROR("CANFDBitTiming requires CANFD communication");
    return {SdkError::INVALID_ARGUMENT,
            "CANFDBitTiming requires CANFD communication"};
  }
  return Connect(device_name, slave_id, static_cast<uint8_t>(timing));
}

Result GHandImpl::PrepareCANFDBaudrateListener(const std::string& device_name,
                                             CANFDBitTiming timing) {
  if (comm_type_ != CommType::CANFD) {
    GHAND_LOG_ERROR("PrepareCANFDBaudrateListener is only supported for CANFD");
    return {SdkError::NOT_SUPPORTED,
            "PrepareCANFDBaudrateListener is only supported for CANFD"};
  }

  const uint8_t baudrate_gear = static_cast<uint8_t>(timing);
  GHAND_LOG_INFO("op=prepare_listener comm=canfd device="
                 << device_name << " gear=0x" << std::hex
                 << static_cast<int>(baudrate_gear) << std::dec);

  bool prepared = false;
  if (device_name == "auto") {
    for (const auto& adapter_pair : SearchAdapters()) {
      if (comm_->PrepareBaudrateListener(adapter_pair.first, baudrate_gear)) {
        prepared = true;
        break;
      }
      GHAND_LOG_ERROR("Failed to prepare CANFD listener: "
                      << adapter_pair.first);
    }
  } else {
    prepared = comm_->PrepareBaudrateListener(device_name, baudrate_gear);
    if (!prepared) {
      GHAND_LOG_ERROR("Failed to prepare CANFD listener: " << device_name);
    }
  }

  if (!prepared) {
    return {SdkError::TRANSPORT_ERROR,
                  device_name == "auto"
                      ? "failed to prepare CANFD listener"
                      : "failed to prepare CANFD listener " + device_name};
  }
  return {};
}

Result GHandImpl::Disconnect() {
  GHAND_LOG_INFO("op=disconnect comm=" << ToString(comm_type_));

  Stop(0);
  int result = comm_->Disconnect();

  if (result == 0) {
    GHAND_LOG_INFO("op=disconnect comm=" << ToString(comm_type_)
                                         << " result=ok");
    return {};
  }
  GHAND_LOG_ERROR("op=disconnect comm=" << ToString(comm_type_)
                                        << " result=transport_error");
  return {SdkError::TRANSPORT_ERROR, "disconnect failed"};
}

bool GHandImpl::IsConnected() const { return comm_->IsConnected(); }

Result GHandImpl::SetSlaveId(uint16_t slave_index, uint8_t new_slave_id) {
  if (!IsConnected()) {
    GHAND_LOG_ERROR("Cannot set slave ID: device not connected");
    return {SdkError::NOT_CONNECTED, "device is not connected"};
  }
  if (!comm_->SetSlaveId(slave_index, new_slave_id)) {
    return {SdkError::DEVICE_REJECTED, "device rejected slave ID change"};
  }
  return {};
}

Result GHandImpl::SetBaudrateConfig(uint16_t slave_index,
                                    RS485BaudRate baud_rate) {
  if (comm_type_ != CommType::RS485) {
    GHAND_LOG_ERROR("RS485BaudRate requires RS485 communication");
    return {SdkError::INVALID_ARGUMENT,
            "RS485BaudRate requires RS485 communication"};
  }
  if (!IsConnected()) {
    GHAND_LOG_ERROR("Cannot set baudrate config: device not connected");
    return {SdkError::NOT_CONNECTED, "device is not connected"};
  }
  if (!comm_->SetBaudrateConfig(slave_index,
                                static_cast<uint8_t>(baud_rate))) {
    return {SdkError::DEVICE_REJECTED,
            "device rejected baudrate configuration"};
  }
  return {};
}

Result GHandImpl::SetBaudrateConfig(uint16_t slave_index,
                                    CANFDBitTiming timing) {
  if (comm_type_ != CommType::CANFD) {
    GHAND_LOG_ERROR("CANFDBitTiming requires CANFD communication");
    return {SdkError::INVALID_ARGUMENT,
            "CANFDBitTiming requires CANFD communication"};
  }
  if (!IsConnected()) {
    GHAND_LOG_ERROR("Cannot set baudrate config: device not connected");
    return {SdkError::NOT_CONNECTED, "device is not connected"};
  }
  if (!comm_->SetBaudrateConfig(slave_index, static_cast<uint8_t>(timing))) {
    return {SdkError::DEVICE_REJECTED,
            "device rejected baudrate configuration"};
  }
  return {};
}

std::map<std::string, std::string> GHandImpl::SearchAdapters() const {
  auto adapters = comm_->SearchAdapters();
  for (const auto& adapter : adapters) {
    GHAND_LOG_DEBUG("Found adapter: " << adapter.first);
  }
  return adapters;
}

ResultOf<HandType> GHandImpl::GetHandType(uint16_t slave_index) const {
  if (slave_index == 0) {
    return {SdkError::INVALID_ARGUMENT,
            "a read names exactly one hand: slave_index 0 is invalid"};
  }
  return ResultOf<HandType>(comm_->GetHandType(slave_index));
}

ResultOf<DeviceInfo> GHandImpl::GetDeviceInfo(uint16_t slave_index) const {
  if (slave_index == 0) {
    return {SdkError::INVALID_ARGUMENT,
            "a read names exactly one hand: slave_index 0 is invalid"};
  }
  return ResultOf<DeviceInfo>(comm_->GetDeviceInfo(slave_index));
}

Result GHandImpl::MoveJoints(uint16_t slave_index,
                             const std::vector<JointCommand>& joints,
                             ControlMode mode) {
  // Check device connection status
  if (!IsConnected()) {
    GHAND_LOG_ERROR("Cannot move joints: device not connected");
    return {SdkError::NOT_CONNECTED, "device is not connected"};
  }

  // Check if command is empty
  if (joints.empty()) {
    GHAND_LOG_WARNING("MoveJoints called with empty joint list");
    return {SdkError::INVALID_ARGUMENT, "joint command list is empty"};
  }

  GHAND_LOG_DEBUG("Moving " << joints.size() << " joints");

  // Sort by valid_joints order, include only controllable joints from user
  // input.
  std::vector<JointCommand> ordered_joints;
  ordered_joints.reserve(joints.size());
  for (const auto& joint_id : config_.valid_joints) {
    if (config_.joint_limits.find(joint_id) == config_.joint_limits.end())
      continue;  // Skip read-only joints
    for (const auto& joint : joints) {
      if (joint.id == joint_id) {
        JointCommand limited = joint;
        ClampJointAngle(limited);
        ClampJointVelocity(limited, mode);
        ClampJointTorque(limited, mode);
        ordered_joints.push_back(limited);
        break;
      }
    }
  }

  if (ordered_joints.empty()) {
    return {SdkError::INVALID_ARGUMENT,
            "joint command list contains no controllable joints"};
  }

  if (!comm_->MoveJoints(slave_index, ordered_joints, mode)) {
    return {SdkError::TRANSPORT_ERROR, "move joints command failed"};
  }
  return {};
}

Result GHandImpl::Stop(uint16_t slave_index) {
  GHAND_LOG_INFO("op=stop comm=" << ToString(comm_type_));
  comm_->Stop(slave_index);
  return {};
}

Result GHandImpl::ClearFault(uint16_t slave_index) {
  GHAND_LOG_INFO("op=clear_fault comm=" << ToString(comm_type_));
  bool result = comm_->ClearFault(slave_index);
  GHAND_LOG_INFO("op=clear_fault comm=" << ToString(comm_type_)
                                        << " result="
                                        << (result ? "ok" : "failed"));
  if (!result) return {SdkError::DEVICE_REJECTED, "clear fault failed"};
  return {};
}

Result GHandImpl::OpenTactile(uint16_t slave_index) {
  if (!comm_->OpenTactile(slave_index)) {
    return {SdkError::DEVICE_REJECTED, "open tactile failed"};
  }
  return {};
}

Result GHandImpl::CloseTactile(uint16_t slave_index) {
  if (!comm_->CloseTactile(slave_index)) {
    return {SdkError::DEVICE_REJECTED, "close tactile failed"};
  }
  return {};
}

Result GHandImpl::ZeroTactile(uint16_t slave_index) {
  if (!comm_->ZeroTactile(slave_index)) {
    return {SdkError::DEVICE_REJECTED, "zero tactile failed"};
  }
  return {};
}

void GHandImpl::SetJointsCallback(
    uint16_t slave_index,
    std::function<void(const std::vector<Joint>& joints)> cb) {
  callback_manager_->SetJointsCallback(slave_index, cb);
}

void GHandImpl::SetHandStateCallback(
    uint16_t slave_index,
    std::function<void(const HandState& state)> cb) {
  callback_manager_->SetHandStateCallback(slave_index, cb);
}

void GHandImpl::SetTactileDataCallback(
    uint16_t slave_index,
    std::function<void(const TactileData& data)> cb) {
  callback_manager_->SetTactileDataCallback(slave_index, cb);
}

ResultOf<HandState> GHandImpl::GetHandData(uint16_t slave_index) const {
  if (slave_index == 0) {
    return {SdkError::INVALID_ARGUMENT,
            "a read names exactly one hand: slave_index 0 is invalid"};
  }
  return ResultOf<HandState>(callback_manager_->GetHandData(slave_index));
}

ResultOf<std::vector<Joint>> GHandImpl::GetJointsData(
    uint16_t slave_index) const {
  if (slave_index == 0) {
    return {SdkError::INVALID_ARGUMENT,
            "a read names exactly one hand: slave_index 0 is invalid"};
  }
  return ResultOf<std::vector<Joint>>(
      callback_manager_->GetJointsData(slave_index));
}

ResultOf<TactileData> GHandImpl::GetTactileData(
    uint16_t slave_index) const {
  if (slave_index == 0) {
    return {SdkError::INVALID_ARGUMENT,
            "a read names exactly one hand: slave_index 0 is invalid"};
  }
  return ResultOf<TactileData>(callback_manager_->GetTactileData(slave_index));
}

ResultOf<SelfTestErrorInfo> GHandImpl::GetSelfTestErrorInfo(
    uint16_t slave_index) const {
  if (slave_index == 0) {
    return {SdkError::INVALID_ARGUMENT,
            "a read names exactly one hand: slave_index 0 is invalid"};
  }
  if (!IsConnected()) {
    GHAND_LOG_ERROR("GetSelfTestErrorInfo called while disconnected");
    return {SdkError::NOT_CONNECTED, "device is not connected"};
  }
  return ResultOf<SelfTestErrorInfo>(comm_->GetSelfTestErrorInfo(slave_index));
}

ResultOf<uint8_t> GHandImpl::GetSelfTestStatus(uint16_t slave_index) const {
  if (slave_index == 0) {
    return {SdkError::INVALID_ARGUMENT,
            "a read names exactly one hand: slave_index 0 is invalid"};
  }
  if (!IsConnected()) {
    GHAND_LOG_ERROR("GetSelfTestStatus called while disconnected");
    return {SdkError::NOT_CONNECTED, "device is not connected"};
  }
  return ResultOf<uint8_t>(comm_->GetSelfTestStatus(slave_index));
}

FirmwareUpdateError GHandImpl::BootUpdate(
                        const std::string& filename,
                        std::function<void(int)> progress_callback) {
  GHAND_LOG_INFO("Starting firmware update: " << filename);

  const int retry_count = 10;
  const int retry_delay_ms = 2000;
  // Firmware update is bus level today, so it reads the first hand's version.
  const std::vector<ConnectedHand> hands = comm_->GetConnectedHands();
  const uint16_t update_slave = hands.empty() ? 0 : hands.front().slave_index;
  std::string last_version =
      comm_->GetDeviceInfo(update_slave).software_version;

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

        FirmwareUpdateError check = CheckFirmwareUpdateResults(results);
        if (check != FirmwareUpdateError::SUCCESS) return check;

        if (!IsVersionNewer(
                last_version,
                comm_->GetDeviceInfo(update_slave).software_version)) {
          GHAND_LOG_WARNING("Firmware version not updated after upgrade");
          // GHAND_LOG_WARNING("Current version: " << comm_->GetDeviceInfo().software_version);
          // GHAND_LOG_WARNING("Last version: " << last_version);
        }

        return FirmwareUpdateError::SUCCESS;
      }
    }
    comm_->Disconnect();
    return FirmwareUpdateError::RECONNECT_FAILED;
  }
  if (comm_type_ == CommType::ETHERCAT &&
      (ret == FirmwareUpdateError::ENTER_BOOT_MODE_FAILED ||
       ret == FirmwareUpdateError::SWITCH_COMM_STATE_FAILED)) {
    comm_->Disconnect();
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

void GHandImpl::ClampJointVelocity(JointCommand& joint, ControlMode mode) {
  if (mode == ControlMode::POSITION) {
    // Position mode: velocity range 0-100, negative values take absolute
    // value, absolute value > 100 clamped to 100.
    if (joint.velocity < 0) {
      int8_t original = joint.velocity;
      joint.velocity = std::abs(joint.velocity);
      GHAND_LOG_WARNING("[Joint] " << ToString(joint.id) << " velocity "
                                   << static_cast<int>(original)
                                   << " is negative in POSITION mode, "
                                   << "converted to absolute value "
                                   << static_cast<int>(joint.velocity));
    }
    if (joint.velocity > 100) {
      int8_t original = joint.velocity;
      joint.velocity = 100;
      GHAND_LOG_WARNING("[Joint] " << ToString(joint.id) << " velocity "
                                   << static_cast<int>(original)
                                   << " exceeds limit in POSITION mode, "
                                   << "clamped to 100");
    }

  } else if (mode == ControlMode::TORQUE) {
    // Torque mode encodes velocity as 0-100.
    int velocity = static_cast<int>(joint.velocity);
    if (velocity > 100) {
      velocity = 100;
    } else if (velocity < -100) {
      velocity = 100;
    } else if (velocity < 0) {
      velocity = -velocity;
    }
    joint.velocity = static_cast<int8_t>(velocity);
  } else if (mode == ControlMode::SPEED) {
    if (joint.velocity < -100) {
      int8_t original = joint.velocity;
      joint.velocity = -100;
      GHAND_LOG_WARNING("[Joint] " << ToString(joint.id) << " velocity "
                                   << static_cast<int>(original)
                                   << " below limit in SPEED mode, "
                                   << "clamped to -100");
    } else if (joint.velocity > 100) {
      int8_t original = joint.velocity;
      joint.velocity = 100;
      GHAND_LOG_WARNING("[Joint] " << ToString(joint.id) << " velocity "
                                   << static_cast<int>(original)
                                   << " exceeds limit in SPEED mode, "
                                   << "clamped to 100");
    }
  }
}

void GHandImpl::ClampJointTorque(JointCommand& joint, ControlMode mode) {
  int original = static_cast<int>(joint.torque);
  int torque = original;

  if (mode == ControlMode::POSITION || mode == ControlMode::SPEED) {
    // Position mode and speed mode: torque range 0-100, negative values take
    // absolute value, absolute value > 100 clamped to 100.
    if (joint.torque < 0) {
      int8_t original = joint.torque;
      int torque_abs = std::abs(static_cast<int>(joint.torque));
      if (torque_abs > 100) torque_abs = 100;
      joint.torque = static_cast<int8_t>(torque_abs);
      const char* mode_name =
          (mode == ControlMode::POSITION) ? "POSITION" : "SPEED";
      GHAND_LOG_WARNING("[Joint] " << ToString(joint.id) << " torque "
                                   << static_cast<int>(original)
                                   << " is negative in " << mode_name
                                   << " mode, converted to absolute value "
                                   << static_cast<int>(joint.torque));
    }
    if (joint.torque > 100) {
      int8_t original = joint.torque;
      joint.torque = 100;
      const char* mode_name =
          (mode == ControlMode::POSITION) ? "POSITION" : "SPEED";
      GHAND_LOG_WARNING("[Joint] " << ToString(joint.id) << " torque "
                                   << static_cast<int>(original)
                                   << " exceeds limit in " << mode_name
                                   << " mode, clamped to 100");
    }
  } else if (mode == ControlMode::TORQUE) {
    // Torque mode: torque range -100 to 100
    if (joint.torque < -100) {
      int8_t original = joint.torque;
      joint.torque = -100;
      GHAND_LOG_WARNING("[Joint] " << ToString(joint.id) << " torque "
                                   << static_cast<int>(original)
                                   << " below limit in TORQUE mode, "
                                   << "clamped to -100");
    } else if (joint.torque > 100) {
      int8_t original = joint.torque;
      joint.torque = 100;
      GHAND_LOG_WARNING("[Joint] " << ToString(joint.id) << " torque "
                                   << static_cast<int>(original)
                                   << " exceeds limit in TORQUE mode, "
                                   << "clamped to 100");
    }
  }

  if (torque != original) {
    joint.torque = static_cast<int8_t>(torque);

    const char* mode_name = (mode == ControlMode::POSITION)
                                ? "POSITION"
                                : (mode == ControlMode::SPEED ? "SPEED" : "TORQUE");

    GHAND_LOG_WARNING("[Joint] "
                      << ToString(joint.id) << " torque " << original
                      << " adjusted to " << torque << " in " << mode_name
                      << " mode");
  }
}

}  // namespace internal
}  // namespace ghand
