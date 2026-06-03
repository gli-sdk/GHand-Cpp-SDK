#define _USE_MATH_DEFINES
#include "dexhand.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
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

namespace {

bool IsVersionNewer(const std::string& old_ver, const std::string& new_ver) {
  auto parse = [](const std::string& ver) {
    std::vector<int> parts;
    std::stringstream ss(ver);
    std::string part;
    while (std::getline(ss, part, '.')) {
      try {
        parts.push_back(std::stoi(part));
      } catch (...) {
        parts.push_back(0);
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
        std::string dev_lower = dev_name;
        std::string cfg_lower = config_.name;
        std::transform(dev_lower.begin(), dev_lower.end(), dev_lower.begin(),
                       ::tolower);
        std::transform(cfg_lower.begin(), cfg_lower.end(), cfg_lower.begin(),
                       ::tolower);
        if (dev_lower != cfg_lower) {
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

  // Sort by valid_joints order, include only controllable joints from user input
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

int DexHand::BootUpdate(const std::string& ifname, uint16_t slave,
                        const std::string& filename,
                        std::function<void(int)> progress_callback) {
  GHAND_LOG_INFO("Starting firmware update: " << filename);

  const int retry_count = 10;
  const int retry_delay_ms = 1000;
  std::string last_version = comm_->GetDeviceInfo().software_version;

  int ret = comm_->BootUpdate(ifname, slave, filename, progress_callback);
  if (ret == 1) {
    for (int i = 0; i < retry_count; i++) {
      progress_callback(100);
      std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));

      if (Connect(device_name_)) {
        if (IsVersionNewer(last_version,
                           comm_->GetDeviceInfo().software_version)) {
          return 1;
        } else {
          return -12;
        }
      }
    }
    comm_->Disconnect();
    return -11;
  } else {
    for (int i = 0; i < retry_count; i++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));

      if (Connect(device_name_)) {
        return ret;
      }
    }
    comm_->Disconnect();
    return ret;
  }
}

void DexHand::ClampJointAngle(JointCommand& joint) {
  auto it = config_.joint_limits.find(joint.id);
  if (it == config_.joint_limits.end()) return;  // No limit found (e.g., DIP joint)

  const float min_angle = it->second.first;
  const float max_angle = it->second.second;
  if (joint.angle < min_angle) {
    GHAND_LOG_WARNING("[Joint] " << ToString(joint.id) << " angle " << joint.angle
                           << " < min " << min_angle << ", set to "
                           << min_angle);
    joint.angle = min_angle;
  } else if (joint.angle > max_angle) {
    GHAND_LOG_WARNING("[Joint] " << ToString(joint.id) << " angle " << joint.angle
                           << " > max " << max_angle << ", set to "
                           << max_angle);
    joint.angle = max_angle;
  }
}

void DexHand::ClampJointVelocity(JointCommand& joint) {
  int original = static_cast<int>(joint.velocity);
  int velocity = original;

  if (control_mode_ == ControlMode::POSITION ||
      control_mode_ == ControlMode::SPEED) {
    // POSITION / SPEED:
    // >100 -> 100, <-100 -> -100, -100~100 directly pass through.
    if (velocity > 100) {
      velocity = 100;
    } else if (velocity < -100) {
      velocity = -100;
    }

  } else if (control_mode_ == ControlMode::TORQUE) {
    // TORQUE:
    // >100 -> 100, <-100 -> 100, -100~100 take absolute value.
    if (velocity > 100) {
      velocity = 100;
    } else if (velocity < -100) {
      velocity = 100;
    } else if (velocity < 0) {
      velocity = -velocity;
    }
  }

  if (velocity != original) {
    joint.velocity = static_cast<int8_t>(velocity);

    const char* mode_name =
        (control_mode_ == ControlMode::POSITION)
            ? "POSITION"
            : (control_mode_ == ControlMode::SPEED ? "SPEED" : "TORQUE");

    GHAND_LOG_WARNING("[Joint] "
                      << ToString(joint.id) << " velocity " << original
                      << " adjusted to " << velocity << " in " << mode_name
                      << " mode");
  }
}

void DexHand::ClampJointTorque(JointCommand& joint) {
  int original = static_cast<int>(joint.torque);
  int torque = original;

  if (control_mode_ == ControlMode::POSITION ||
      control_mode_ == ControlMode::TORQUE) {
    // POSITION / TORQUE:
    // >100 -> 100, <-100 -> -100, -100~100 directly pass through.
    if (torque > 100) {
      torque = 100;
    } else if (torque < -100) {
      torque = -100;
    }

  } else if (control_mode_ == ControlMode::SPEED) {
    // SPEED:
    // >100 -> 100, <-100 -> 100, -100~100 directly pass through.
    if (torque > 100) {
      torque = 100;
    } else if (torque < -100) {
      torque = 100;
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
