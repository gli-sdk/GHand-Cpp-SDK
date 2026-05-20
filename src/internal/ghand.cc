#define _USE_MATH_DEFINES
#include "ghand.h"

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
      LOG_ERROR("Failed to load product config for product type: "
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
      LOG_WARNING("Unsupported communication type");
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
  LOG_INFO("Connecting to device: " << device_name);

  int result = comm_->Connect(device_name);
  if (result == 0) {
    device_name_ = device_name;
    LOG_INFO("Successfully connected to device: " << device_name);

    // 连接后校验 / 自动识别
    std::string dev_name = comm_->GetDeviceInfo().device_name;
    if (!dev_name.empty()) {
      if (product_type_ == ProductType::AUTO) {
        config_ = FindConfigByName(dev_name);
        if (config_.name.empty()) {
          LOG_ERROR("Auto-detection failed for device: " << dev_name);
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
          LOG_ERROR("Failed to reconnect after auto-detection");
          return false;
        }
        LOG_INFO("Auto-detected product: " << config_.name);
      } else if (!config_.name.empty()) {
        std::string dev_lower = dev_name;
        std::string cfg_lower = config_.name;
        std::transform(dev_lower.begin(), dev_lower.end(), dev_lower.begin(),
                       ::tolower);
        std::transform(cfg_lower.begin(), cfg_lower.end(), cfg_lower.begin(),
                       ::tolower);
        if (dev_lower != cfg_lower) {
          LOG_WARNING("Device name mismatch: device reports \""
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
  LOG_INFO("Disconnecting from device");

  Stop();
  int result = comm_->Disconnect();

  if (result == 0) {
    LOG_INFO("Successfully disconnected from device");
  } else {
    LOG_ERROR("Failed to disconnect from device");
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
  // 检查设备连接状态
  if (!IsConnected()) {
    LOG_ERROR("Cannot move joints: device not connected");
    return false;
  }

  // 检查命令是否为空
  if (joints.empty()) {
    LOG_WARNING("MoveJoints called with empty joint list");
    return false;
  }

  LOG_DEBUG("Moving " << joints.size() << " joints");

  // 按 valid_joints 顺序排列，仅包含可控关节（有 joint_limits 的）
  std::map<JointId, JointCommand> joint_map;
  for (const auto& joint : joints) {
    JointCommand limited_joint = joint;
    ClampJointAngle(limited_joint);
    ClampJointVelocity(limited_joint);
    ClampJointTorque(limited_joint);
    joint_map[joint.id] = limited_joint;
  }

  std::vector<JointCommand> ordered_joints;
  ordered_joints.reserve(config_.joint_limits.size());
  for (const auto& joint_id : config_.valid_joints) {
    if (config_.joint_limits.find(joint_id) == config_.joint_limits.end())
      continue;  // 跳过只读关节
    auto it = joint_map.find(joint_id);
    if (it != joint_map.end()) {
      ordered_joints.push_back(it->second);
    } else {
      float default_angle = 0.0f;
      auto limit = config_.joint_limits.find(joint_id);
      if (limit != config_.joint_limits.end() && limit->second.first > 0.0f) {
        default_angle = limit->second.first;
      }
      ordered_joints.push_back({joint_id, default_angle, 0, 0});
    }
  }

  return comm_->MoveJoints(ordered_joints, control_mode_);
}

void DexHand::Stop() {
  LOG_INFO("Sending stop command");
  comm_->Stop();
}

bool DexHand::ClearFault() {
  LOG_INFO("Clearing device fault");
  return comm_->ClearFault();
}

bool DexHand::InitJoint() {
  LOG_INFO("Initializing joint positions");
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
                        std::function<void(int)> progressCallback) {
  LOG_INFO("Starting firmware update: " << filename);

  const int retry_count = 10;
  const int retry_delay_ms = 1000;
  std::string last_version = comm_->GetDeviceInfo().software_version;

  int ret = comm_->BootUpdate(ifname, slave, filename, progressCallback);
  if (ret == 1) {
    for (int i = 0; i < retry_count; i++) {
      progressCallback(100);
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
  if (it == config_.joint_limits.end()) return;  // 未找到限制(如DIP关节)

  const float min_angle = it->second.first;
  const float max_angle = it->second.second;
  if (joint.angle < min_angle) {
    LOG_WARNING("[Joint] " << ToString(joint.id) << " angle " << joint.angle
                           << " < min " << min_angle << ", set to "
                           << min_angle);
    joint.angle = min_angle;
  } else if (joint.angle > max_angle) {
    LOG_WARNING("[Joint] " << ToString(joint.id) << " angle " << joint.angle
                           << " > max " << max_angle << ", set to "
                           << max_angle);
    joint.angle = max_angle;
  }
}

void DexHand::ClampJointVelocity(JointCommand& joint) {
  if (control_mode_ == ControlMode::POSITION) {
    // 位置模式：速度范围 0-100，负数取绝对值，绝对值>100取100
    if (joint.velocity < 0) {
      int8_t original = joint.velocity;
      joint.velocity = abs(joint.velocity);
      LOG_WARNING(
          "[Joint] "
          << ToString(joint.id) << " velocity " << static_cast<int>(original)
          << " is negative in POSITION mode, converted to absolute value "
          << static_cast<int>(joint.velocity));
    }
    if (joint.velocity > 100) {
      int8_t original = joint.velocity;
      joint.velocity = 100;
      LOG_WARNING("[Joint] "
                  << ToString(joint.id) << " velocity "
                  << static_cast<int>(original)
                  << " exceeds limit in POSITION mode, clamped to 100");
    }
  } else if (control_mode_ == ControlMode::SPEED) {
    // 速度模式：速度范围 -100到100
    if (joint.velocity < -100) {
      int8_t original = joint.velocity;
      joint.velocity = -100;
      LOG_WARNING("[Joint] " << ToString(joint.id) << " velocity "
                             << static_cast<int>(original)
                             << " below limit in SPEED mode, clamped to -100");
    } else if (joint.velocity > 100) {
      int8_t original = joint.velocity;
      joint.velocity = 100;
      LOG_WARNING("[Joint] " << ToString(joint.id) << " velocity "
                             << static_cast<int>(original)
                             << " exceeds limit in SPEED mode, clamped to 100");
    }
  }
  // 力矩模式：速度不影响，不进行检查
}

void DexHand::ClampJointTorque(JointCommand& joint) {
  if (control_mode_ == ControlMode::POSITION ||
      control_mode_ == ControlMode::SPEED) {
    // 位置模式和速度模式：力矩范围 0-100，负数取绝对值，绝对值>100取100
    if (joint.torque < 0) {
      int8_t original = joint.torque;
      joint.torque = abs(joint.torque);
      const char* mode_name =
          (control_mode_ == ControlMode::POSITION) ? "POSITION" : "SPEED";
      LOG_WARNING("[Joint] " << ToString(joint.id) << " torque "
                             << static_cast<int>(original) << " is negative in "
                             << mode_name
                             << " mode, converted to absolute value "
                             << static_cast<int>(joint.torque));
    }
    if (joint.torque > 100) {
      int8_t original = joint.torque;
      joint.torque = 100;
      const char* mode_name =
          (control_mode_ == ControlMode::POSITION) ? "POSITION" : "SPEED";
      LOG_WARNING("[Joint] " << ToString(joint.id) << " torque "
                             << static_cast<int>(original)
                             << " exceeds limit in " << mode_name
                             << " mode, clamped to 100");
    }
  } else if (control_mode_ == ControlMode::TORQUE) {
    // 力矩模式：力矩范围 -100到100
    if (joint.torque < -100) {
      int8_t original = joint.torque;
      joint.torque = -100;
      LOG_WARNING("[Joint] " << ToString(joint.id) << " torque "
                             << static_cast<int>(original)
                             << " below limit in TORQUE mode, clamped to -100");
    } else if (joint.torque > 100) {
      int8_t original = joint.torque;
      joint.torque = 100;
      LOG_WARNING("[Joint] "
                  << ToString(joint.id) << " torque "
                  << static_cast<int>(original)
                  << " exceeds limit in TORQUE mode, clamped to 100");
    }
  }
}

}  // namespace internal
}  // namespace ghand
