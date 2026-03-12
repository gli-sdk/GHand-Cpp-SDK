#define _USE_MATH_DEFINES
#include "xiaoyao/dexhand.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

namespace xiaoyao {

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

DexHand::DexHand() {
    ethercat_comm_ = std::unique_ptr<EtherCATComm>(new EtherCATComm());

    // 注册PDO数据回调
    EtherCATComm::SetDataCallback(
        std::bind(&DexHand::OnRawDataReceived, this,
                  std::placeholders::_1, std::placeholders::_2));
}

DexHand::~DexHand() {}

/**
 * @brief Auto connect the hand
 *
 * @param comm_type Communication type
 * @return bool true: success, false: fail
 */
/**
 * @brief 统一的设备连接辅助方法
 *
 * @param comm_type 通信类型
 * @param device_name 设备名称
 * @return bool true: 成功，false: 失败
 */
bool DexHand::ConnectToDevice(CommType comm_type, const std::string& device_name) {
    switch (comm_type) {
        case CommType::ETHERCAT: {
            int ec_result = ethercat_comm_->Connect(device_name);
            if (ec_result == 0) {
                GetDeviceInfo();
                GetHandType();
                return true;
            }
            return false;
        }
        case CommType::CANFD:
        case CommType::RS485:
        default:
            return false;
    }
}

/**
 * @brief Auto connect the hand
 *
 * @param comm_type Communication type
 * @return bool true: success, false: fail
 */
bool DexHand::AutoConnect(CommType comm_type) {
    if (comm_type == CommType::ETHERCAT) {
        std::map<std::string, std::string> adapter_names = SearchAdapters();
        for (const auto& adapter_pair : adapter_names) {
            if (ConnectToDevice(comm_type, adapter_pair.first)) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief Connect to the hand
 *
 * @param comm_type Communication type
 * @param device_name Device name
 * @return bool true: success, false: fail
 */
bool DexHand::Connect(CommType comm_type, const std::string& device_name) {
    if (device_name == "auto") {
        return AutoConnect(comm_type);
    }
    return ConnectToDevice(comm_type, device_name);
}

/**
 * @brief Disconnect from the hand
 *
 * @return bool true: success, false: fail
 */
bool DexHand::Disconnect() {
    hand_type_ = HandType::NONE;
    device_info_ = DeviceInfo();
    int result = ethercat_comm_->Disconnect();
    return (result == 0);
}

/**
 * @brief List the adapters
 *
 */
std::map<std::string, std::string> DexHand::SearchAdapters() const {
    return ethercat_comm_->SearchAdapters();
}



/**
 * @brief Move the joints with parameterized commands
 *
 * @param joints Vector of joint commands
 * @return bool true: success, false: fail
 */
bool DexHand::MoveJoints(const std::vector<JointCommand>& joints) {
    // 检查设备连接状态
    if (!IsConnected()) {
        return false;
    }

    // 检查命令是否为空
    if (joints.empty()) {
        return false;
    }

    // 将用户传入的joints转为unordered_map，按id索引（方便快速查找）
    std::unordered_map<int, JointCommand> joint_map;
    for (const auto& joint : joints) {
        if (static_cast<int>(joint.id) < static_cast<int>(JointId::NUM_JOINTS)) {
            joint_map[static_cast<int>(joint.id)] = joint;
        }
    }

    // 构建发送缓冲区
    uint8_t buffer[80] = {0};
    size_t offset = 0;

    // 写入控制模式
    uint8_t mode = static_cast<uint8_t>(control_mode_);
    memcpy(buffer + offset, &mode, sizeof(mode));
    offset += sizeof(mode);

    // 写入停止标志
    uint8_t stop = 0;
    memcpy(buffer + offset, &stop, sizeof(stop));
    offset += sizeof(stop);

    // 按照JointId枚举顺序（0-17）遍历所有关节
    // 硬件期望按固定顺序接收数据
    for (int i = 0; i < static_cast<int>(JointId::NUM_JOINTS); i++) {
        // 跳过所有DIP关节（硬件不支持控制）
        if (i == static_cast<int>(JointId::THUMB_DIP) ||
            i == static_cast<int>(JointId::FF_DIP) ||
            i == static_cast<int>(JointId::MF_DIP) ||
            i == static_cast<int>(JointId::RF_DIP) ||
            i == static_cast<int>(JointId::LF_DIP)) {
            continue;
        }

        // 从map中查找当前关节的数据
        float angle = 0.0f;
        uint8_t velocity = 0;
        uint8_t torque = 0;

        auto it = joint_map.find(i);
        if (it != joint_map.end()) {
            // 找到了用户传入的数据
            angle = it->second.angle;
            velocity = it->second.velocity;
            torque = it->second.torque;
        } else {
            // 未找到，使用默认值0
            angle = 0.0f;
            velocity = 0;
            torque = 0;
        }

        // 转换角度（与原实现保持一致）
        if (i == static_cast<int>(JointId::THUMB_ROTATION)) {
            angle = (angle + 30) * (static_cast<float>(M_PI) / 180.0f);
        } else {
            angle = angle * (static_cast<float>(M_PI) / 180.0f);
        }

        // 写入角度（4字节）
        memcpy(buffer + offset, &angle, sizeof(angle));
        offset += sizeof(angle);

        // 写入速度（1字节）
        memcpy(buffer + offset, &velocity, sizeof(velocity));
        offset += sizeof(velocity);

        // 写入力矩（1字节）
        memcpy(buffer + offset, &torque, sizeof(torque));
        offset += sizeof(torque);
    }

    int wkc = ethercat_comm_->SendRxPDO(1, ECT_SDO_RXPDOASSIGN, sizeof(buffer), buffer);

    return true;  // 返回成功或失败
}

/**
 * @brief Set the control mode
 *
 * @param mode Control mode (POSITION, TORQUE, SPEED)
 */
void DexHand::SetControlMode(ControlMode mode) {
    control_mode_ = mode;
}

/**
 * @brief Stop the joints
 *
 */
void DexHand::Stop() {
    // 构造停止命令
    uint8_t buffer[80] = {0};
    buffer[1] = 1;
    ethercat_comm_->SendRxPDO(1, ECT_SDO_RXPDOASSIGN, sizeof(buffer), buffer);
}

/**
 * @brief Get the joints
 *
 * @return int 0: success other:fail
 */
DeviceInfo DexHand::GetDeviceInfo() const {
    // 如果软件版本已读取（非空），说明已缓存
    if (!device_info_.software_version.empty()) {
        return device_info_;
    }

    // 从硬件读取
    std::uint8_t value[255] = {0};
    int size = sizeof(value);
    int result = -1;

    result = ethercat_comm_->SDORead(1, 0x1008, 0x00, &size, &value, EC_TIMEOUTRXM);
    if (result == 1) {
        device_info_.device_name = std::string(reinterpret_cast<char*>(value));
    }

    memset(value, 0, sizeof(value));
    result = ethercat_comm_->SDORead(1, 0x1009, 0x00, &size, &value, EC_TIMEOUTRXM);
    if (result == 1) {
        device_info_.hardware_version = std::string(reinterpret_cast<char*>(value));
    }

    memset(value, 0, sizeof(value));
    result = ethercat_comm_->SDORead(1, 0x100A, 0x00, &size, &value, EC_TIMEOUTRXM);
    if (result == 1) {
        device_info_.software_version = std::string(reinterpret_cast<char*>(value));
    }

    memset(value, 0, sizeof(value));
    result = ethercat_comm_->SDORead(1, 0x1018, 0x04, &size, &value, EC_TIMEOUTRXM);
    if (result == 1) {
        device_info_.serial_number = std::string(reinterpret_cast<char*>(value));
    }

    return device_info_;
}

/**
 * @brief Get the hand type
 *
 * @param slave Slave ID
 */
HandType DexHand::GetHandType() const {
    // 如果已识别，直接返回缓存
    if (hand_type_ != HandType::NONE) {
        return hand_type_;
    }

    // 从硬件读取
    std::uint8_t value = 0;
    int size = sizeof(value);
    int result = ethercat_comm_->SDORead(1, 0x2001, 0x00, &size, &value, EC_TIMEOUTRXM);

    if (result == 1) {
        switch (value) {
            case 0:
                hand_type_ = HandType::NONE;
                break;
            case 1:
                hand_type_ = HandType::LEFT;
                break;
            case 2:
                hand_type_ = HandType::RIGHT;
                break;
            default:
                hand_type_ = HandType::NONE;
                break;
        }
    }

    return hand_type_;
}

bool DexHand::IsConnected() const {
    return ethercat_comm_ && ethercat_comm_->IsConnected();
}

/**
 * @brief Release protection
 *
 * @return bool true: success, false: fail
 */
bool DexHand::ClearFault() {
    std::uint8_t command = 0x01;
    std::uint8_t state = 0xFF;
    int size = sizeof(std::uint8_t);
    std::uint8_t result = 0;  // 初始化为0
    int ret = -1;
    int times = 100;
    ret = ethercat_comm_->SDOWrite(1, 0x2002, 0x01, size, &command, EC_TIMEOUTRXM);
    if (ret > 0) {
        while (times--) {
            ret = ethercat_comm_->SDORead(1, 0x2002, 0x02, &size, &state, EC_TIMEOUTRXM);
            if (ret > 0 && state == 0) {  // 执行完成
                ret = ethercat_comm_->SDORead(1, 0x2002, 0x03, &size, &result, EC_TIMEOUTRXM);
                return result == 1;  // true成功, false失败
            }
        }
    }
    return false;
}

/**
 * @brief Initialize joints
 *
 * @return bool true: success, false: fail
 */
bool DexHand::InitJoint() {
    std::uint8_t command = 0x01;
    std::uint8_t state = 0xFF;
    int size = sizeof(std::uint8_t);
    std::uint8_t result = 0;  // 初始化为0
    int ret = -1;
    int times = 100;
    ret = ethercat_comm_->SDOWrite(1, 0x2003, 0x01, size, &command, EC_TIMEOUTRXM);
    if (ret > 0) {
        while (times--) {
            ret = ethercat_comm_->SDORead(1, 0x2003, 0x02, &size, &state, EC_TIMEOUTRXM);
            if (ret > 0 && state == 0) {  // 执行完成
                ret = ethercat_comm_->SDORead(1, 0x2003, 0x03, &size, &result, EC_TIMEOUTRXM);
                return result == 1;  // true成功, false失败
            }
        }
    }
    return false;
}

bool DexHand::OpenTactile() {
    std::uint8_t command = 0x01;
    int size = sizeof(std::uint8_t);
    int result = -1;
    result = ethercat_comm_->SDOWrite(1, 0x2004, 0x01, size, &command, EC_TIMEOUTRXM);
    if (result > 0) {
        return true;
    }
    return false;
}

bool DexHand::CloseTactile() {
    std::uint8_t command = 0x02;
    int size = sizeof(std::uint8_t);
    int result = -1;
    result = ethercat_comm_->SDOWrite(1, 0x2004, 0x01, size, &command, EC_TIMEOUTRXM);
    if (result > 0) {
        return true;
    }
    return false;
}

/**
 * @brief Reset tactile
 *
 * @return int 1: success, other: fail
 */
bool DexHand::ZeroTactile() {
    std::uint8_t command = 0x04;
    int size = sizeof(std::uint8_t);
    int result = -1;
    result = ethercat_comm_->SDOWrite(1, 0x2004, 0x01, size, &command, EC_TIMEOUTRXM);
    if (result > 0) {
        return true;
    }
    return false;
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
    const int retry_count = 10;
    const int retry_delay_ms = 1000;
    std::string last_version = device_info_.software_version;
    std::uint8_t command = 0x5A;
    std::uint8_t state = 0xFF;
    std::uint8_t result = 0xFF;
    int size = sizeof(std::uint8_t);
    int ret = -1;
    ret = ethercat_comm_->SDOWrite(1, 0x2005, 0x01, size, &command, EC_TIMEOUTRXM);
    if (ret > 0) {
        ret = ethercat_comm_->SDORead(1, 0x2005, 0x02, &size, &state, EC_TIMEOUTRXM);
        if (ret > 0 && state == 0) {  // 执行完成
            ret = ethercat_comm_->SDORead(1, 0x2005, 0x03, &size, &result, EC_TIMEOUTRXM);
        }
    }
    if (result == 1)  // 1成功,2失败
    {
        ret = ethercat_comm_->BootUpdate(ifname.c_str(), slave, filename.c_str(), progressCallback);
        if (ret == 1) {
            for (int i = 0; i < retry_count; i++) {
                progressCallback(100);
                std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));

                if (Connect(CommType::ETHERCAT, ifname)) {
                    if (last_version < device_info_.software_version) {
                        return 1;
                    } else {
                        return -12;
                    }
                }
            }
            return -11;
        } else {
            for (int i = 0; i < retry_count; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));

                if (Connect(CommType::ETHERCAT, ifname)) {
                    return ret;
                }
            }
            return ret;
        }
    }
    return 0;
}

// PDO数据回调处理方法
void DexHand::OnRawDataReceived(const uint8_t* data, size_t size) {
    // 直接解析PDO数据（与GetJoints()相同的解析逻辑）
    std::vector<Joint> parsed_joints;
    HandTemperature parsed_temperature;

    if (data == nullptr || size == 0) {
        return;  // 无效数据
    }

    size_t offset = 0;
    uint8_t hand_state, hand_error;
    int16_t temperature;

    // 解析手部状态和温度
    memcpy(&hand_state, data + offset, sizeof(hand_state));
    offset += sizeof(hand_state);

    memcpy(&hand_error, data + offset, sizeof(hand_error));
    offset += sizeof(hand_error);

    memcpy(&temperature, data + offset, sizeof(temperature));
    offset += sizeof(temperature);

    parsed_temperature.state = static_cast<State>(hand_state);
    parsed_temperature.error = static_cast<ErrorCode>(hand_error);
    parsed_temperature.temperature = temperature;

    // 解析关节数据
    parsed_joints.resize(static_cast<int>(JointId::NUM_JOINTS));
    for (int i = 0; i < static_cast<int>(JointId::NUM_JOINTS); i++) {
        uint8_t joint_state, joint_error;
        float joint_angle;
        uint8_t joint_velocity, joint_torque;

        memcpy(&joint_state, data + offset, sizeof(joint_state));
        offset += sizeof(joint_state);

        memcpy(&joint_error, data + offset, sizeof(joint_error));
        offset += sizeof(joint_error);

        memcpy(&joint_angle, data + offset, sizeof(joint_angle));
        offset += sizeof(joint_angle);

        memcpy(&joint_velocity, data + offset, sizeof(joint_velocity));
        offset += sizeof(joint_velocity);

        memcpy(&joint_torque, data + offset, sizeof(joint_torque));
        offset += sizeof(joint_torque);

        parsed_joints[i].id = static_cast<JointId>(i);
        parsed_joints[i].state = static_cast<State>(joint_state);
        parsed_joints[i].error = static_cast<ErrorCode>(joint_error);
        joint_angle = joint_angle * (180.0f / static_cast<float>(M_PI));
        if (i == static_cast<int>(JointId::THUMB_ROTATION)) {
            joint_angle = joint_angle - 30;
        }
        parsed_joints[i].angle = joint_angle;
        parsed_joints[i].velocity = joint_velocity;
        parsed_joints[i].torque = joint_torque;
    }

    // 触发回调（纯回调模式，SDK不保存缓存）
    callback_manager_.UpdateJoints(parsed_joints);
    callback_manager_.UpdateTemperature(parsed_temperature);

    // 解析触觉数据并触发回调
    // 触觉数据紧跟在关节数据后面，无需硬编码偏移量
    // 格式：[拇指合力6B][拇指分布力156B][食指合力6B][食指分布力93B]...
    if (offset < size) {  // 确保 offset 小于总大小
        TactileData tactile_data;

        for (int finger_idx = static_cast<int>(FingerType::THUMB);
             finger_idx < static_cast<int>(FingerType::NUM_FINGERS);
             finger_idx++) {
            FingerType finger = static_cast<FingerType>(finger_idx);

            // 1. 解析合力数据（固定 6 字节）
            if (offset + 6 <= size) {
                Force resultant_force = ParseResultantForce(data + offset);
                offset += 6;
                tactile_data.resultants[finger_idx] = resultant_force;
            }

            // 2. 解析分布力数据（可变长度）
            int sensor_count = GetTactileSensorCount(finger);
            int sample_size = sensor_count * 3;

            if (offset + sample_size <= size) {
                std::vector<Force> sample_forces = ParseSampleForces(data + offset, sensor_count);
                offset += sample_size;
                tactile_data.samples[finger_idx] = std::move(sample_forces);
            }
        }

        // 触发一次性回调，包含所有触觉数据
        if (tactile_data.HasData()) {
            callback_manager_.UpdateTactileData(tactile_data);
        }
    }
}

}  // namespace xiaoyao
