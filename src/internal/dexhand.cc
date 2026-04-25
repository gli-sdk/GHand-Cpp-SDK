#define _USE_MATH_DEFINES
#include "dexhand.h"
#include "canfd_comm.h"
#include "ethercat_comm.h"
#include "dexhand_callback_manager.h"
#include "logger.h"

#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace xiaoyao {
namespace internal {

// ===== 触觉数据解析辅助函数（内部使用） =====
namespace {

/**
 * @brief 解析合力数据
 * @param data PDO 数据缓冲区（至少 6 字节）
 * @return 合力向量
 */
Force ParseResultantForce(const uint8_t* data) {
    Force resultant;
    resultant.x = 0.0f;
    resultant.y = 0.0f;
    resultant.z = 0.0f;

    if (data != nullptr) {
        int16_t raw_x = static_cast<int16_t>(data[0] | (data[1] << 8));
        resultant.x = static_cast<float>(static_cast<int8_t>(raw_x & 0xFF)) * 0.1f;

        int16_t raw_y = static_cast<int16_t>(data[2] | (data[3] << 8));
        resultant.y = static_cast<float>(static_cast<int8_t>(raw_y & 0xFF)) * 0.1f;

        uint16_t raw_z = static_cast<uint16_t>(data[4] | (data[5] << 8));
        resultant.z = static_cast<float>(static_cast<uint8_t>(raw_z & 0xFF)) * 0.1f;
    }

    return resultant;
}

/**
 * @brief 解析分布力数据
 * @param data PDO 数据缓冲区
 * @param sensor_count 传感器数量（大拇指 52，其他 31）
 * @return 分布力向量数组
 */
std::vector<Force> ParseSampleForces(const uint8_t* data, int sensor_count) {
    std::vector<Force> forces;
    forces.reserve(sensor_count);

    if (data != nullptr && sensor_count > 0) {
        for (int i = 0; i < sensor_count; i++) {
            Force sample_force;
            sample_force.x = static_cast<float>(static_cast<int8_t>(data[i * 3])) * 0.1f;
            sample_force.y = static_cast<float>(static_cast<int8_t>(data[i * 3 + 1])) * 0.1f;
            sample_force.z = static_cast<float>(static_cast<uint8_t>(data[i * 3 + 2])) * 0.1f;
            forces.push_back(sample_force);
        }
    }

    return forces;
}

/**
 * @brief 获取手指的分布力传感器数量
 * @param finger 手指类型
 * @return 传感器数量（大拇指 52，其他 31）
 */
int GetTactileSensorCount(FingerType finger) {
    return (finger == FingerType::THUMB) ? 52 : 31;
}

}  // anonymous namespace

DexHand::DexHand(CommType comm_type)
    : hand_type_(HandType::NONE),
      control_mode_(ControlMode::POSITION),
      comm_type_(comm_type),
      device_name_("") {
    switch (comm_type) {
        case CommType::ETHERCAT:
            comm_ = std::unique_ptr<EtherCATComm>(new EtherCATComm());
            break;
        case CommType::CANFD:
            comm_ = std::unique_ptr<CANFDComm>(new CANFDComm());
            break;
        case CommType::RS485:
        default:
            LOG_WARNING("Unsupported communication type");
            break;
    }
    callback_manager_ = std::unique_ptr<DexHandCallbackManager>(new DexHandCallbackManager());

    SetupCallbacks();
}

DexHand::~DexHand() = default;

void DexHand::SetupCallbacks() {
    if (!comm_) return;
    comm_->SetJointsCallback(
        [this](const std::vector<Joint>& joints) {
            callback_manager_->UpdateJoints(joints);
        });
    comm_->SetHandStateCallback(
        [this](const HandState& state) {
            callback_manager_->UpdateTemperature(state);
        });
    comm_->SetTactileDataCallback(
        [this](const TactileData& data) {
            callback_manager_->UpdateTactileData(data);
        });
}

bool DexHand::ConnectToDevice(const std::string& device_name) {
    LOG_INFO("Connecting to device: " << device_name);

    /*switch (comm_type_) {
        case CommType::ETHERCAT: {
            comm_ = std::unique_ptr<EtherCATComm>(new EtherCATComm());
            break;
        }
        case CommType::CANFD: {
            comm_ = std::unique_ptr<CANFDComm>(new CANFDComm());
            break;
        }
        case CommType::RS485:
        default:
            LOG_WARNING("Unsupported communication type");
            return false;
    }

    SetupCallbacks();*/

    int result = comm_->Connect(device_name);
    if (result == 0) {
        device_name_ = device_name;
        GetDeviceInfo();
        GetHandType();
        LOG_INFO("Successfully connected to device: " << device_name);
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

    hand_type_ = HandType::NONE;
    device_info_ = DeviceInfo();
    Stop();
    int result = comm_->Disconnect();

    if (result == 0) {
        LOG_INFO("Successfully disconnected from device");
    } else {
        LOG_ERROR("Failed to disconnect from device");
    }

    return (result == 0);
}

bool DexHand::IsConnected() const {
    return comm_ && comm_->IsConnected();
}

std::map<std::string, std::string> DexHand::SearchAdapters() const {
    return comm_->SearchAdapters();
}

HandType DexHand::GetHandType() const {
    hand_type_ = comm_->GetHandType();
    return hand_type_;
}

DeviceInfo DexHand::GetDeviceInfo() const {
    device_info_ = comm_->GetDeviceInfo();
    return device_info_;
}

void DexHand::SetControlMode(ControlMode mode) {
    control_mode_ = mode;
}

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

    // 构建完整关节命令表（全部 18 个关节），未传入的补 0
    std::vector<JointCommand> full_joints;
    full_joints.reserve(static_cast<int>(JointId::NUM_JOINTS));
    for (int i = 0; i < static_cast<int>(JointId::NUM_JOINTS); ++i) {
        full_joints.push_back({static_cast<JointId>(i), 0.0f, 0, 0});
    }

    for (const auto& joint : joints) {
        int idx = static_cast<int>(joint.id);
        if (idx >= 0 && idx < static_cast<int>(JointId::NUM_JOINTS)) {
            JointCommand limited_joint = joint;
            ClampJointAngle(limited_joint);
            ClampJointVelocity(limited_joint);
            ClampJointTorque(limited_joint);
            full_joints[idx] = limited_joint;
        }
    }

    return comm_->MoveJoints(full_joints, control_mode_);
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

bool DexHand::OpenTactile() {
    return comm_->OpenTactile();
}

bool DexHand::CloseTactile() {
    return comm_->CloseTactile();
}

bool DexHand::ZeroTactile() {
    return comm_->ZeroTactile();
}

void DexHand::SetJointsCallback(std::function<void(const std::vector<Joint>&)> cb) {
    callback_manager_->SetJointsCallback(cb);
}

void DexHand::SetHandStateCallback(std::function<void(const HandState&)> cb) {
    callback_manager_->SetHandStateCallback(cb);
}

void DexHand::SetTactileDataCallback(std::function<void(const TactileData&)> cb) {
    callback_manager_->SetTactileDataCallback(cb);
}

/**
 * @brief Boot update firmware
 *
 * @param ifname Network interface name
 * @param slave Slave station number
 * @param filename Firmware file path
 * @param progressCallback Progress callback function used to report update progress percentage
 * @return int Update result: 1-success, -11-connection timeout, -12-version not updated, other
 * values indicate update failure
 */
int DexHand::BootUpdate(const std::string& ifname, uint16_t slave,
                        const std::string& filename,
                        std::function<void(int)> progressCallback) {
    LOG_INFO("Starting firmware update: " << filename);

    const int retry_count = 10;
    const int retry_delay_ms = 1000;
    std::string last_version = device_info_.software_version;

    int ret = comm_->BootUpdate(ifname, slave, filename, progressCallback);
    if (ret == 1) {
        for (int i = 0; i < retry_count; i++) {
            progressCallback(100);
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));

            if (Connect(device_name_)) {
                if (last_version < device_info_.software_version) {
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

// 关节限制静态表 (仅包含可控关节, 单位: 度)
const std::map<JointId, std::pair<float, float>> DexHand::kJointLimits_ = {
    // 大拇指
    {JointId::THUMB_PIP, {0.0f, 66.0f}},
    {JointId::THUMB_MCP, {0.0f, 50.0f}},
    {JointId::THUMB_SWING, {20.0f, 90.0f}},
    {JointId::THUMB_ROTATION, {-10.0f, 60.0f}},
    // 食指
    {JointId::FF_PIP, {0.0f, 80.0f}},
    {JointId::FF_MCP, {0.0f, 90.0f}},
    {JointId::FF_SWING, {-10.0f, 10.0f}},
    // 中指
    {JointId::MF_PIP, {0.0f, 90.0f}},
    {JointId::MF_MCP, {0.0f, 90.0f}},
    // 无名指
    {JointId::RF_PIP, {0.0f, 90.0f}},
    {JointId::RF_MCP, {0.0f, 90.0f}},
    // 小指
    {JointId::LF_PIP, {0.0f, 74.0f}},
    {JointId::LF_MCP, {0.0f, 90.0f}}
};

void DexHand::ClampJointAngle(JointCommand& joint) {
    auto it = kJointLimits_.find(joint.id);
    if (it == kJointLimits_.end()) return;  // 未找到限制(如DIP关节)

    const float min_angle = it->second.first;
    const float max_angle = it->second.second;
    if (joint.angle < min_angle) {
        LOG_WARNING("[Joint] " << ToString(joint.id)
                   << " angle " << joint.angle << " < min " << min_angle << ", set to " << min_angle);
        joint.angle = min_angle;
    } else if (joint.angle > max_angle) {
        LOG_WARNING("[Joint] " << ToString(joint.id)
                   << " angle " << joint.angle << " > max " << max_angle << ", set to " << max_angle);
        joint.angle = max_angle;
    }
}

void DexHand::ClampJointVelocity(JointCommand& joint) {
    if (control_mode_ == ControlMode::POSITION) {
        // 位置模式：速度范围 0-100，负数取绝对值，绝对值>100取100
        if (joint.velocity < 0) {
            int8_t original = joint.velocity;
            joint.velocity = abs(joint.velocity);
            LOG_WARNING("[Joint] " << ToString(joint.id)
                       << " velocity " << static_cast<int>(original)
                       << " is negative in POSITION mode, converted to absolute value "
                       << static_cast<int>(joint.velocity));
        }
        if (joint.velocity > 100) {
            int8_t original = joint.velocity;
            joint.velocity = 100;
            LOG_WARNING("[Joint] " << ToString(joint.id)
                       << " velocity " << static_cast<int>(original)
                       << " exceeds limit in POSITION mode, clamped to 100");
        }
    } else if (control_mode_ == ControlMode::SPEED) {
        // 速度模式：速度范围 -100到100
        if (joint.velocity < -100) {
            int8_t original = joint.velocity;
            joint.velocity = -100;
            LOG_WARNING("[Joint] " << ToString(joint.id)
                       << " velocity " << static_cast<int>(original)
                       << " below limit in SPEED mode, clamped to -100");
        } else if (joint.velocity > 100) {
            int8_t original = joint.velocity;
            joint.velocity = 100;
            LOG_WARNING("[Joint] " << ToString(joint.id)
                       << " velocity " << static_cast<int>(original)
                       << " exceeds limit in SPEED mode, clamped to 100");
        }
    }
    // 力矩模式：速度不影响，不进行检查
}

void DexHand::ClampJointTorque(JointCommand& joint) {
    if (control_mode_ == ControlMode::POSITION || control_mode_ == ControlMode::SPEED) {
        // 位置模式和速度模式：力矩范围 0-100，负数取绝对值，绝对值>100取100
        if (joint.torque < 0) {
            int8_t original = joint.torque;
            joint.torque = abs(joint.torque);
            const char* mode_name = (control_mode_ == ControlMode::POSITION) ? "POSITION" : "SPEED";
            LOG_WARNING("[Joint] " << ToString(joint.id)
                       << " torque " << static_cast<int>(original)
                       << " is negative in " << mode_name << " mode, converted to absolute value "
                       << static_cast<int>(joint.torque));
        }
        if (joint.torque > 100) {
            int8_t original = joint.torque;
            joint.torque = 100;
            const char* mode_name = (control_mode_ == ControlMode::POSITION) ? "POSITION" : "SPEED";
            LOG_WARNING("[Joint] " << ToString(joint.id)
                       << " torque " << static_cast<int>(original)
                       << " exceeds limit in " << mode_name << " mode, clamped to 100");
        }
    } else if (control_mode_ == ControlMode::TORQUE) {
        // 力矩模式：力矩范围 -100到100
        if (joint.torque < -100) {
            int8_t original = joint.torque;
            joint.torque = -100;
            LOG_WARNING("[Joint] " << ToString(joint.id)
                       << " torque " << static_cast<int>(original)
                       << " below limit in TORQUE mode, clamped to -100");
        } else if (joint.torque > 100) {
            int8_t original = joint.torque;
            joint.torque = 100;
            LOG_WARNING("[Joint] " << ToString(joint.id)
                       << " torque " << static_cast<int>(original)
                       << " exceeds limit in TORQUE mode, clamped to 100");
        }
    }
}

}  // namespace internal
}  // namespace xiaoyao
