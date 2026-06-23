#define _USE_MATH_DEFINES
#include "dexhand.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#include "canfd_comm.h"
#include "dexhand_callback_manager.h"
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

}  // anonymous namespace

namespace ghand {
namespace internal {

DexHand::DexHand(ProductType product_type, CommType comm_type)
    : control_mode_(ControlMode::POSITION),
      comm_type_(comm_type),
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
      std::unique_ptr<DexHandCallbackManager>(new DexHandCallbackManager());
  if (product_type != ProductType::AUTO) {
    SetupCallbacks();
  }
}

DexHand::~DexHand() = default;

void DexHand::SetupCallbacks() {
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

bool DexHand::ConnectToDevice(const std::string& device_name) {
  GHAND_LOG_INFO("Connecting to device: " << device_name);

  int result = comm_->Connect(device_name);
  if (result == 0) {
    device_name_ = device_name;
    GHAND_LOG_INFO("Successfully connected to device: " << device_name);

    // Post-connection verification / auto-detection
    std::string dev_name = comm_->GetDeviceInfo().device_name;
    if (!dev_name.empty()) {
      if (product_type_ == ProductType::AUTO) {
        config_ = FindConfigByName(dev_name);
        if (config_.name.empty()) {
          GHAND_LOG_ERROR("Auto-detection failed for device: " << dev_name);
          comm_->Disconnect();
          return false;
        }
        comm_->Disconnect();
        switch (comm_type_) {
          case CommType::ETHERCAT:
            comm_.reset(new EtherCATComm(config_));
            break;
          case CommType::CANFD:
            comm_.reset(new CANFDComm(config_));
            break;
          case CommType::RS485:
            comm_.reset(new RS485Comm(config_));
            break;
          default:
            break;
        }
        SetupCallbacks();
        int rc = comm_->Connect(device_name);
        if (rc != 0) {
          GHAND_LOG_ERROR("Failed to reconnect after auto-detection");
          return false;
        }
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
          GHAND_LOG_WARNING("Device name mismatch: device reports \""
                      << dev_name << "\", config expects \"" << config_.name
                      << "\"");
        }
      }
    }

    return true;
  }
  return false;
}

bool DexHand::AutoConnect() {
  std::map<std::string, std::string> adapter_names = SearchAdapters();
  for (const auto& adapter_pair : adapter_names) {
    if (ConnectToDevice(adapter_pair.first)) {
      return true;
    }
  }
  return false;
}

bool DexHand::Connect(const std::string& device_name) {
  if (device_name == "auto") {
    return AutoConnect();
  }
  return ConnectToDevice(device_name);
}

bool DexHand::Disconnect() {
  GHAND_LOG_INFO("Disconnecting from device");

  Stop();
  int result = comm_->Disconnect();

  if (result == 0) {
    GHAND_LOG_INFO("Successfully disconnected from device");
  } else {
    GHAND_LOG_ERROR("Failed to disconnect from device");
  }

  return (result == 0);
}

bool DexHand::IsConnected() const { return comm_->IsConnected(); }

std::map<std::string, std::string> DexHand::SearchAdapters() const {
  return comm_->SearchAdapters();
}

HandType DexHand::GetHandType() const { return comm_->GetHandType(); }

DeviceInfo DexHand::GetDeviceInfo() const { return comm_->GetDeviceInfo(); }

void DexHand::SetControlMode(ControlMode mode) { control_mode_ = mode; }

bool DexHand::MoveJoints(const std::vector<JointCommand>& joints) {
  // Check device connection status
  if (!IsConnected()) {
    GHAND_LOG_ERROR("Cannot move joints: device not connected");
    return false;
  }

  // Check if command is empty
  if (joints.empty()) {
    GHAND_LOG_WARNING("MoveJoints called with empty joint list");
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
    for (const auto& joint : joints) {
      if (joint.id == joint_id) {
        JointCommand limited = joint;
        ClampJointAngle(limited);
        ClampJointVelocity(limited);
        ClampJointTorque(limited);
        ordered_joints.push_back(limited);
        break;
      }
    }
  }

  return comm_->MoveJoints(ordered_joints, control_mode_);
}

void DexHand::Stop() {
  GHAND_LOG_INFO("Sending stop command");
  comm_->Stop();
}

bool DexHand::ClearFault() {
  GHAND_LOG_INFO("Clearing device fault");
  return comm_->ClearFault();
}

bool DexHand::InitJoint() {
  GHAND_LOG_INFO("Initializing joint positions");
  return comm_->InitJoint();
}

bool DexHand::OpenTactile() { return comm_->OpenTactile(); }

bool DexHand::CloseTactile() { return comm_->CloseTactile(); }

bool DexHand::ZeroTactile() { return comm_->ZeroTactile(); }

void DexHand::SetJointsCallback(
    std::function<void(const std::vector<Joint>&)> cb) {
  callback_manager_->SetJointsCallback(cb);
}

void DexHand::SetHandStateCallback(std::function<void(const HandState&)> cb) {
  callback_manager_->SetHandStateCallback(cb);
}

void DexHand::SetTactileDataCallback(
    std::function<void(const TactileData&)> cb) {
  callback_manager_->SetTactileDataCallback(cb);
}

HandState DexHand::GetHandData() const {
  return callback_manager_->GetHandData();
}

std::vector<Joint> DexHand::GetJointsData() const {
  return callback_manager_->GetJointsData();
}

TactileData DexHand::GetTactileData() const {
  return callback_manager_->GetTactileData();
}

FirmwareUpdateError DexHand::BootUpdate(
                        const std::string& filename,
                        std::function<void(int)> progress_callback) {
  GHAND_LOG_INFO("Starting firmware update: " << filename);

  const int retry_count = 10;
  const int retry_delay_ms = 1000;
  std::string last_version = comm_->GetDeviceInfo().software_version;

  FirmwareUpdateError ret =
      comm_->BootUpdate(filename, progress_callback);
  if (ret == FirmwareUpdateError::SUCCESS) {
    for (int i = 0; i < retry_count; i++) {
      progress_callback(100);
      std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));

      if (Connect(device_name_)) {
        uint8_t main_result = 0, pos_result = 0, tac_result = 0,
                motor_result = 0;
        if (!comm_->QueryFirmwareUpdateResults(
                &main_result, &pos_result, &tac_result, &motor_result)) {
          return FirmwareUpdateError::QUERY_FAILED;
        }

        GHAND_LOG_INFO("Firmware update results - main: "
                       << static_cast<int>(main_result)
                       << ", pos: " << static_cast<int>(pos_result)
                       << ", tac: " << static_cast<int>(tac_result)
                       << ", motor: " << static_cast<int>(motor_result));

        if (main_result != 1)
          return FirmwareUpdateError::MAIN_CONTROLLER_FAILED;
        if (pos_result != 1)
          return FirmwareUpdateError::POSITION_SENSOR_FAILED;
        if (tac_result != 1)
          return FirmwareUpdateError::TACTILE_SENSOR_FAILED;
        if (motor_result != 1)
          return FirmwareUpdateError::MOTOR_DRIVER_FAILED;

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
  } else {
    for (int i = 0; i < retry_count; i++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));

      if (Connect(device_name_)) {
        return ret;
      }
    }
    comm_->Disconnect();
    return FirmwareUpdateError::RECONNECT_FAILED;
  }
}

void DexHand::ClampJointAngle(JointCommand& joint) {
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

void DexHand::ClampJointVelocity(JointCommand& joint) {
  if (control_mode_ == ControlMode::POSITION) {
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

  } else if (control_mode_ == ControlMode::TORQUE) {
    // TORQUE:
    // >100 -> 100, <-100 -> 100, -100~100 take absolute value.
    int velocity = static_cast<int>(joint.velocity);
    if (velocity > 100) {
      velocity = 100;
    } else if (velocity < -100) {
      velocity = 100;
    } else if (velocity < 0) {
      velocity = -velocity;
    }
    joint.velocity = static_cast<int8_t>(velocity);
  } else if (control_mode_ == ControlMode::SPEED) {
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

void DexHand::ClampJointTorque(JointCommand& joint) {
  int original = static_cast<int>(joint.torque);
  int torque = original;

  if (control_mode_ == ControlMode::POSITION ||
      control_mode_ == ControlMode::SPEED) {
    // Position mode and speed mode: torque range 0-100, negative values take
    // absolute value, absolute value > 100 clamped to 100.
    if (joint.torque < 0) {
      int8_t original = joint.torque;
      int torque_abs = std::abs(static_cast<int>(joint.torque));
      if (torque_abs > 100) torque_abs = 100;
      joint.torque = static_cast<int8_t>(torque_abs);
      const char* mode_name =
          (control_mode_ == ControlMode::POSITION) ? "POSITION" : "SPEED";
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
          (control_mode_ == ControlMode::POSITION) ? "POSITION" : "SPEED";
      GHAND_LOG_WARNING("[Joint] " << ToString(joint.id) << " torque "
                                   << static_cast<int>(original)
                                   << " exceeds limit in " << mode_name
                                   << " mode, clamped to 100");
    }
  } else if (control_mode_ == ControlMode::TORQUE) {
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

    const char* mode_name =
        (control_mode_ == ControlMode::POSITION)
            ? "POSITION"
            : (control_mode_ == ControlMode::SPEED ? "SPEED" : "TORQUE");

    GHAND_LOG_WARNING("[Joint] "
                      << ToString(joint.id) << " torque " << original
                      << " adjusted to " << torque << " in " << mode_name
                      << " mode");
  }
}

}  // namespace internal
}  // namespace ghand
