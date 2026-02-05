#define _USE_MATH_DEFINES
#include "xiaoyao/dexhand.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

template <typename T>
std::string FormatFieldWithBytes(const uint8_t* buffer, size_t offset,
                                 const std::string& field_name, T value);
DexHand::DexHand() {
    ethercat_comm_ = std::unique_ptr<EtherCATComm>(new EtherCATComm());
    InitializeAllJoints();
    EtherCATComm::SetStateUpdateCallback(
        std::bind(&DexHand::OnSlaveStateUpdate, this, std::placeholders::_1));
}

DexHand::~DexHand() {}

void DexHand::InitializeAllJoints() {
    joints_.clear();
    for (int i = 0; i < NUM_JOINTS; i++) {
        Joint joint;
        joint.id = static_cast<JointId>(i);
        joint.target.angle = 0.0f;
        joint.target.velocity = 50;
        joint.target.torque = 100;

        joint.state.state = 0;
        joint.state.error = 0;
        joint.state.angle = 0.0f;
        joint.state.velocity = 0;
        joint.state.torque = 0;

        joints_.push_back(joint);
    }
}
/**
 * @brief Open the hand
 *
 * @param comm_type Communication type
 * @param device_name Device name
 * @return int 0: success, -1: fail
 */
int DexHand::Open(CommType comm_type, std::string device_name) {
    // auto now = std::chrono::system_clock::now();
    // auto time_t = std::chrono::system_clock::to_time_t(now);
    // std::stringstream ss;
    // ss << "comm_log/log_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".txt";
    // log_file_ = new LogFile(ss.str());
    // log_file_->WriteLog("Connect to the hand...");
    int result = -1;
    //     log_file_->WriteLog("comm_type: " + std::to_string(comm_type) +
    //                                             "   Device name: " + device_name);
    switch (comm_type) {
        case COMM_ETHERCAT:
            if (device_name == "auto") {
                result = AutoConnectDevices();
            } else {
                result = ConnectDevice(device_name);
            }
            break;
        case COMM_CANFD:
            break;
        case COMM_RS485:
            break;
        default:
            break;
    }
    return result;
}

/**
 * @brief Close the hand
 *
 * @return int 0
 */
int DexHand::Close() {
    ethercat_comm_->Disconnect();
    connect_state_ = DISCONNECT;
    InitializeAllJoints();
    return 0;
}

/**
 * @brief Auto connect the hand
 *
 * @return int i: success, -1: fail
 */
int DexHand::AutoConnectDevices() {
    ListAdapters();
    int index = 0;
    // log_file_->WriteLog("Adapter count: " + std::to_string(adapter_names_.size()));
    for (const auto& adapter_pair : adapter_names_) {
        // log_file_->WriteLog("Adapter: " + adapter_pair.first + " " + adapter_pair.second);
        if (ConnectDevice(adapter_pair.first) == 0) {
            return index;
        }
        index++;
    }
    return -1;
}

/**
 * @brief List the adapters
 *
 */
void DexHand::ListAdapters() {
    adapter_names_.clear();
    adapter_names_ = ethercat_comm_->ListAdapters();
}

/**
 * @brief Connect the hand
 *
 * @param device_name Device name
 * @return int 0: success, -1: fail
 */
int DexHand::ConnectDevice(std::string device_name) {
    // log_file_->WriteLog(device_name);
    int result = ethercat_comm_->Connect(device_name);
    if (result == 0) {
        connect_state_ = CONNECT;
        operation_mode_ = MODE_NORMAL;
        GetDeviceInfo(1);
        GetHandType(1);
        // log_file_->WriteLog("Connected");
        // log_file_->WriteLog("Hand type: " + std::to_string(hand_type_));
    }
    // log_file_->WriteLog("Result:" + std::to_string(result));
    return result;
}

/**
 * @brief Move the joints
 *
 * @return int 0: success
 */
int DexHand::MoveJoints() {
    if (is_move_joints_ == 0) {
        return 0;
    }

    // auto now = std::chrono::system_clock::now();
    // auto time_t = std::chrono::system_clock::to_time_t(now);
    // auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) %
    // 1000;

    // 格式化时间戳
    // std::ostringstream timeStream;
    // timeStream << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    // timeStream << '.' << std::setfill('0') << std::setw(3) << ms.count();

    // 添加带时间戳的日志
    // log_file_->WriteLog("[" + timeStream.str() + "] SendRxPDO:");
    uint8_t buffer[80] = {0};
    size_t offset = 0;

    uint8_t mode = 0;
    uint8_t stop = 0;
    // log_file_->WriteLog("Mode: " + std::to_string(mode) + "Stop: " + std::to_string(stop));

    memcpy(buffer + offset, &mode, sizeof(mode));
    offset += sizeof(mode);

    memcpy(buffer + offset, &stop, sizeof(stop));
    offset += sizeof(stop);

    for (const auto& joint : joints_) {
        if (joint.id >= NUM_JOINTS || joint.id == THUMB_DIP || joint.id == FF_DIP ||
            joint.id == MF_DIP || joint.id == RF_DIP || joint.id == LF_DIP) {
            continue;
        }
        float angle = 0.0;
        if (joint.id == THUMB_ROTATION) {
            angle = (joint.target.angle + 30) * (M_PI / 180.0f);
        } else {
            angle = (joint.target.angle) * (M_PI / 180.0f);
        }

        uint8_t velocity = joint.target.velocity;
        uint8_t torque = joint.target.torque;

        memcpy(buffer + offset, &angle, sizeof(angle));
        offset += sizeof(angle);

        memcpy(buffer + offset, &velocity, sizeof(velocity));
        offset += sizeof(velocity);

        memcpy(buffer + offset, &torque, sizeof(torque));
        offset += sizeof(torque);

        // log_file_->WriteLog("Angle: " + std::to_string(angle) + "   Speed:" +
        //                     std::to_string(velocity) + "   Torque:" + std::to_string(torque));
    }
    // std::ostringstream hexStream;
    // hexStream << "SendBuffer: ";
    // for (size_t i = 0; i < sizeof(buffer); ++i) {
    //     hexStream << std::hex << std::setw(2) << std::setfill('0') <<
    //     static_cast<int>(buffer[i]); if (i % 6 == 1)
    //         hexStream << "\n";
    //     else
    //         hexStream << " ";
    // }
    // log_file_->WriteLog(hexStream.str());
    int wkc = ethercat_comm_->SendRxPDO(1, ECT_SDO_RXPDOASSIGN, sizeof(buffer), buffer);
    return 0;
}

/**
 * @brief Get the joints
 *
 * @return int 0: success other:fail
 */
int DexHand::GetJoints() {
    uint8_t* inputs = ethercat_comm_->ReadTxPDO(1);
    if (inputs == nullptr) {
        return -1;
    }
    int result = 0;
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    // 格式化时间戳
    // std::ostringstream timeStream;
    // timeStream << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    // timeStream << '.' << std::setfill('0') << std::setw(3) << ms.count();

    // 添加带时间戳的日志
    // log_file_->WriteLog("[" + timeStream.str() + "] ReadTxPDO:");
    // std::ostringstream hexStream;
    // hexStream << "RecvBuffer: ";
    // for (size_t i = 0; i < 208; ++i) {
    //     hexStream << std::hex << std::setw(2) << std::setfill('0') <<
    //     static_cast<int>(inputs[i]); if (i % 8 == 3)
    //         hexStream << "\n";
    //     else
    //         hexStream << " ";
    // }
    // log_file_->WriteLog(hexStream.str());

    size_t offset = 0;
    uint8_t hand_state, hand_error;
    int16_t temperature;

    memcpy(&hand_state, inputs + offset, sizeof(hand_state));
    offset += sizeof(hand_state);

    memcpy(&hand_error, inputs + offset, sizeof(hand_error));
    offset += sizeof(hand_error);

    if ((hand_state == 2 || hand_state == 3) && hand_error != 0) {
        result = -2;
        // return hand_error;
    }

    memcpy(&temperature, inputs + offset, sizeof(temperature));
    offset += sizeof(temperature);

    hand_temperature_.state = hand_state;
    hand_temperature_.error = hand_error;
    hand_temperature_.temperature = temperature;

    // log_file_->WriteLog("State: " + std::to_string(hand_state) +
    //                     "   Error:" + std::to_string(hand_error) +
    //                     "   Temperature: " + std::to_string(temperature));

    for (int i = 0; i < NUM_JOINTS; i++) {
        uint8_t joint_state, joint_error;
        float joint_angle;
        uint8_t joint_velocity, joint_torque;

        memcpy(&joint_state, inputs + offset, sizeof(joint_state));
        offset += sizeof(joint_state);

        memcpy(&joint_error, inputs + offset, sizeof(joint_error));
        offset += sizeof(joint_error);

        if ((joint_state == 2 || joint_state == 3) && joint_error != 0) {
            // return joint_error;
            result = -2;
        }

        memcpy(&joint_angle, inputs + offset, sizeof(joint_angle));
        offset += sizeof(joint_angle);

        memcpy(&joint_velocity, inputs + offset, sizeof(joint_velocity));
        offset += sizeof(joint_velocity);

        memcpy(&joint_torque, inputs + offset, sizeof(joint_torque));
        offset += sizeof(joint_torque);

        joints_[i].state.state = joint_state;
        joints_[i].state.error = joint_error;
        joint_angle = joint_angle * (180.0f / M_PI);
        if (i == THUMB_ROTATION) {
            joint_angle = joint_angle - 30;
        }
        joints_[i].state.angle = joint_angle;
        joints_[i].state.velocity = joint_velocity;
        joints_[i].state.torque = joint_torque;

        // log_file_->WriteLog("State: " + std::to_string(joint_state) +
        //                     "   error:" + std::to_string(joint_error) +
        //                     "   Angle: " + std::to_string(joint_angle) +
        //                     "   Speed:" + std::to_string(joint_velocity) +
        //                     "   Torque:" + std::to_string(joint_torque));
    }

    return result;
}
void DexHand::GetDeviceInfo(std::uint16_t slave) {
    device_info_.device_name = "";
    device_info_.hardware_version = "";
    device_info_.serial_number = "";
    device_info_.software_version = "";

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
}

/**
 * @brief Get the hand type
 *
 * @param slave Slave ID
 */
void DexHand::GetHandType(std::uint16_t slave) {
    std::uint8_t value = 0;
    int size = sizeof(value);
    int result = -1;
    result = ethercat_comm_->SDORead(1, 0x2001, 0x00, &size, &value, EC_TIMEOUTRXM);
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
    } else {
        hand_type_ = HandType::NONE;
    }
}

/**
 * @brief Release protection
 *
 * @return int 1: success, other: fail
 */
int DexHand::ReleaseProtection() {
    std::uint8_t command = 0x01;
    std::uint8_t state = 0xFF;
    int size = sizeof(std::uint8_t);
    std::uint8_t result = -1;
    int ret = -1;
    int times = 100;
    ret = ethercat_comm_->SDOWrite(1, 0x2002, 0x01, size, &command, EC_TIMEOUTRXM);
    if (ret > 0) {
        while (times--) {
            ret = ethercat_comm_->SDORead(1, 0x2002, 0x02, &size, &state, EC_TIMEOUTRXM);
            if (ret > 0 && state == 0) {  // 执行完成
                ret = ethercat_comm_->SDORead(1, 0x2002, 0x03, &size, &result, EC_TIMEOUTRXM);
                return result;  // 1成功,2失败
            }
        }
    }
    return -1;
}

/**
 * @brief Initialize joints
 *
 * @return int 1: success, other: fail
 */
int DexHand::InitJoint() {
    std::uint8_t command = 0x01;
    std::uint8_t state = 0xFF;
    int size = sizeof(std::uint8_t);
    std::uint8_t result = -1;
    int ret = -1;
    int times = 100;
    ret = ethercat_comm_->SDOWrite(1, 0x2003, 0x01, size, &command, EC_TIMEOUTRXM);
    if (ret > 0) {
        while (times--) {
            ret = ethercat_comm_->SDORead(1, 0x2003, 0x02, &size, &state, EC_TIMEOUTRXM);
            if (ret > 0 && state == 0) {  // 执行完成
                ret = ethercat_comm_->SDORead(1, 0x2003, 0x03, &size, &result, EC_TIMEOUTRXM);
                if (result == 1)  // 1成功,2失败
                    break;
                else
                    return result;
            }
        }
    }
    return -1;
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
bool DexHand::ResetTactile() {
    std::uint8_t command = 0x03;
    int size = sizeof(std::uint8_t);
    int result = -1;
    result = ethercat_comm_->SDOWrite(1, 0x2004, 0x01, size, &command, EC_TIMEOUTRXM);
    if (result > 0) {
        return true;
    }
    return false;
}

bool DexHand::ResetToZero() {
    std::uint8_t command = 0x04;
    int size = sizeof(std::uint8_t);
    int result = -1;
    result = ethercat_comm_->SDOWrite(1, 0x2004, 0x01, size, &command, EC_TIMEOUTRXM);
    if (result > 0) {
        return true;
    }
    return false;
}

int DexHand::GetResultantForce(FingerType finger_type, std::vector<Force>* resultant_forces) {
    uint8_t* inputs = ethercat_comm_->ReadTxPDO(1);
    if (inputs == nullptr) {
        return -1;
    }
    uint8_t state, error;

    memcpy(&state, inputs + 148, 1);
    memcpy(&error, inputs + 149, 1);

    // int offset[5] = {230, 392, 491, 590, 689};
    if (finger_type == NUM_FINGERS) {
        for (int finger_idx = THUMB; finger_idx < NUM_FINGERS; finger_idx++) {
            FingerType current_finger = static_cast<FingerType>(finger_idx);
            GetResultantForce(current_finger, resultant_forces);
        }
    } else {
        uint8_t extracted_data[6] = {0};
        Finger* finger = new Finger(finger_type);
        int offset = finger->GetResultantForceOffset();
        int size = finger->GetResultantForceSize();

        memcpy(extracted_data, inputs + offset, size);
        Force force = finger->GetResultantForce(extracted_data, size);
        delete finger;
        resultant_forces->push_back(force);
        return 0;
    }
    return 0;
}
// 获取单个手指分布力
int DexHand::GetSampleForce(FingerType finger_type, std::vector<Force>* sample_forces) {
    uint8_t* inputs = ethercat_comm_->ReadTxPDO(1);
    if (inputs == nullptr) {
        return -1;
    }

    Finger* finger = new Finger(finger_type);
    int offset = finger->GetSampleForcesOffset();
    int size = finger->GetSampleForcesSize();

    std::vector<uint8_t> extracted_data(size * 3, 0);
    memcpy(extracted_data.data(), inputs + offset, size * 3);
    std::vector<Force> forces = finger->GetSampleForces(extracted_data.data(), size);
    sample_forces->insert(sample_forces->end(), forces.begin(), forces.end());
    delete finger;
    return 0;
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
int DexHand::BootUpdate(char* ifname, uint16_t slave, char* filename,
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
        ret = ethercat_comm_->BootUpdate(ifname, slave, filename, progressCallback);
        if (ret == 1) {
            for (int i = 0; i < retry_count; i++) {
                progressCallback(100);
                std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));

                if (Open(COMM_ETHERCAT, ifname) >= 0) {
                    if (last_version < device_info_.software_version) {
                        return 1;
                    } else {
                        return -12;
                    }
                }
            }
            connect_state_ = DISCONNECT;
            return -11;
        } else {
            for (int i = 0; i < retry_count; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));

                if (Open(COMM_ETHERCAT, ifname) >= 0) {
                    connect_state_ = CONNECT;
                    return ret;
                }
            }
            connect_state_ = DISCONNECT;
            return ret;
        }
    }
    return 0;
}

template <typename T>
std::string FormatFieldWithBytes(const uint8_t* buffer, size_t offset,
                                 const std::string& field_name, T value) {
    std::ostringstream oss;
    oss << field_name << ": ";

    // 输出字节内容
    for (size_t i = 0; i < sizeof(T); ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[offset + i])
            << " ";
    }

    // 输出解析值
    oss << "-> " << value;
    return oss.str();
}
