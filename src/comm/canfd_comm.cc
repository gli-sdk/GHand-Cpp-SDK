#ifdef _WIN32
    #define _USE_MATH_DEFINES
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "canfd_comm.h"
#include "logger.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ghand {
namespace internal {

// 每关节数据尺寸:uint16 angle(2B) + uint8 velocity(1B) + uint8 torque(1B)
constexpr size_t kCanfdJointDataSize = 4;

CANFDComm::CANFDComm(const ProductConfig& config)
    : driver_(CreateZLGDriver()), config_(config) {}

CANFDComm::~CANFDComm() {
    Disconnect();
}

// device_name: ZLG 设备标识符，支持以下格式：
//   1) "ZCAN_USBCANFD_100U:0:0"  标准 ZLG 格式（型号:设备索引:通道索引）
//   2) "42:0:0"                  数字型号格式
// 亦可通过 hand.SearchAdapters() 枚举可用设备后使用返回的 key 连接。
int CANFDComm::Connect(const std::string& device_name) {
    LOG_INFO("CANFD connecting to: " << device_name);

    if (!driver_) {
        LOG_ERROR("CANFD driver not available");
        return -1;
    }

    if (driver_->Open(device_name, 1000000, 5000000) != 0) {
        LOG_ERROR("Failed to open CANFD channel: " << device_name);
        return -2;
    }

    connected_.store(true);
    rx_running_ = true;
    rx_thread_ = std::thread(&CANFDComm::ReceiveThread, this);

    // 读取真实设备 ID（覆盖默认 0x71）
    std::vector<uint8_t> resp;
    if (SendRecvCmd(canfd::GET_SLAVE_ID, nullptr, 0, &resp, 1000) && !resp.empty()) {
        device_id_ = resp[0];
        LOG_INFO("CANFD device ID detected: 0x" << std::hex << static_cast<int>(device_id_));
    } else {
        LOG_WARNING("Failed to read device ID, using default 0x71");
    }

    // 订阅主动上报
    SubscribeActiveReport(canfd::REPORT_JOINTS, 10);   // 10ms = 100Hz
    if (config_.has_tactile) {
        SubscribeActiveReport(canfd::REPORT_TACTILE, 50);  // 50ms = 20Hz
    }

    return 0;
}

int CANFDComm::Disconnect() {
    LOG_INFO("CANFD disconnecting");

    if (IsConnected()) {
        UnsubscribeActiveReport(canfd::REPORT_JOINTS);
        if (config_.has_tactile) {
            UnsubscribeActiveReport(canfd::REPORT_TACTILE);
        }
    }

    connected_.store(false);
    rx_running_ = false;
    if (rx_thread_.joinable()) {
        rx_thread_.join();
    }

    if (driver_) {
        driver_->Close();
    }

    {
        std::lock_guard<std::mutex> lock(response_mutex_);
        response_slots_.clear();
        response_cv_.notify_all();
    }

    LOG_INFO("CANFD disconnected");
    return 0;
}

std::map<std::string, std::string> CANFDComm::SearchAdapters() {
    if (driver_) {
        return driver_->EnumerateAdapters();
    }
    return {};
}

// ===== 设备信息 =====

DeviceInfo CANFDComm::GetDeviceInfo() {
    DeviceInfo info;
    std::vector<uint8_t> resp;

    if (SendRecvCmd(canfd::GET_DEVICE_NAME, nullptr, 0, &resp)) {
        info.device_name = std::string(reinterpret_cast<char*>(resp.data()), resp.size());
    }
    if (SendRecvCmd(canfd::GET_HW_VERSION, nullptr, 0, &resp)) {
        info.hardware_version = std::string(reinterpret_cast<char*>(resp.data()), resp.size());
    }
    if (SendRecvCmd(canfd::GET_SW_VERSION, nullptr, 0, &resp)) {
        info.software_version = std::string(reinterpret_cast<char*>(resp.data()), resp.size());
    }
    if (SendRecvCmd(canfd::GET_SERIAL, nullptr, 0, &resp) && resp.size() >= 4) {
        info.serial_number =
            static_cast<unsigned int>(resp[0]) |
            (static_cast<unsigned int>(resp[1]) << 8) |
            (static_cast<unsigned int>(resp[2]) << 16) |
            (static_cast<unsigned int>(resp[3]) << 24);
    }

    return info;
}

HandType CANFDComm::GetHandType() {
    std::vector<uint8_t> resp;
    if (SendRecvCmd(canfd::GET_HAND_TYPE, nullptr, 0, &resp) && !resp.empty()) {
        switch (resp[0]) {
            case 1: return HandType::LEFT;
            case 2: return HandType::RIGHT;
            default: return HandType::NONE;
        }
    }
    return HandType::NONE;
}

// ===== 运动控制 =====

bool CANFDComm::MoveJoints(const std::vector<JointCommand>& joints,
                           ControlMode mode) {
    if (!IsConnected()) {
        LOG_ERROR("Cannot move joints: device not connected");
        return false;
    }
    if (joints.empty()) {
        LOG_WARNING("MoveJoints called with empty joint list");
        return false;
    }

    // 构建 63B 参数 (CANFD 单帧 payload 上限，不含功能码)
    // 布局与固件约定保持一致：
    //   Byte[0]  data[1] : 模式+急停
    //   Byte[1]  data[2] : 电机索引+控制数量
    //   Byte[2]  data[3] : 大拇指 PIP 角度低字节
    //   Byte[3]  data[4] : 大拇指 PIP 角度高字节
    //   Byte[4]  data[5] : 大拇指 PIP 速度
    //   Byte[5]  data[6] : 大拇指 PIP 力矩
    //   ... 按 JointId 枚举顺序依次正序填充 ...
    //   Byte[52] data[53]: 小指 MCP 力矩
    //   Byte[53..62]     : 0，满足 CANFD 物理层要求
    uint8_t param[63] = {0};
    param[0] = static_cast<uint8_t>(mode);
    param[1] = 13;  // 从 THUMB_PIP 开始，共 13 个可控关节

    size_t buf_offset = 2;
    for (int i = 0; i < static_cast<int>(JointId::NUM_JOINTS); ++i) {
        if (buf_offset + 3 >= sizeof(param)) break;

        JointId id = static_cast<JointId>(i);
        // 跳过 DIP 关节（不可控）
        if (id == JointId::THUMB_DIP ||
            id == JointId::FF_DIP ||
            id == JointId::MF_DIP ||
            id == JointId::RF_DIP ||
            id == JointId::LF_DIP) {
            continue;
        }

        // 查找该关节是否有命令（传入列表已按 DexHand 补全为 18 个）
        float angle = 0.0f;
        uint8_t velocity = 0;
        uint8_t torque = 0;
        for (const auto& joint : joints) {
            if (joint.id == id) {
                angle = joint.angle;
                velocity = joint.velocity;
                torque = joint.torque;
                break;
            }
        }

        uint16_t raw_angle = AngleToRaw(angle, id);
        param[buf_offset++] = static_cast<uint8_t>(raw_angle & 0xFF);
        param[buf_offset++] = static_cast<uint8_t>((raw_angle >> 8) & 0xFF);
        param[buf_offset++] = velocity;
        param[buf_offset++] = torque;
    }

    return SendCmdOnly(canfd::CONTROL_JOINTS, param, sizeof(param));
}

void CANFDComm::Stop() {
    if (!IsConnected()) return;

    uint8_t param[63] = {0};
    param[0] = 0x01;  // 急停标志

    SendCmdOnly(canfd::CONTROL_JOINTS, param, sizeof(param));
}

// ===== 系统操作 =====

bool CANFDComm::ClearFault() {
    if (!SendCmdOnly(canfd::CLEAR_FAULT, nullptr, 0)) {
        return false;
    }

    // 轮询状态直到完成
    int retries = 100;
    while (retries-- > 0) {
        std::vector<uint8_t> state_resp, error_resp;
        if (SendRecvCmd(canfd::GET_BOARD_STATE, nullptr, 0, &state_resp) &&
            SendRecvCmd(canfd::GET_BOARD_ERROR, nullptr, 0, &error_resp)) {
            if (!state_resp.empty() && state_resp[0] == 0 &&
                !error_resp.empty() && error_resp[0] == 0) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    LOG_ERROR("Clear fault operation timed out");
    return false;
}

bool CANFDComm::InitJoint() {
    if (!SendCmdOnly(canfd::INIT_JOINT, nullptr, 0)) {
        return false;
    }

    int retries = 100;
    while (retries-- > 0) {
        std::vector<uint8_t> state_resp, error_resp;
        if (SendRecvCmd(canfd::GET_BOARD_STATE, nullptr, 0, &state_resp) &&
            SendRecvCmd(canfd::GET_BOARD_ERROR, nullptr, 0, &error_resp)) {
            if (!state_resp.empty() && state_resp[0] == 0 &&
                !error_resp.empty() && error_resp[0] == 0) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    LOG_ERROR("Joint initialization timed out");
    return false;
}

// ===== 触觉传感器 =====

bool CANFDComm::OpenTactile() {
    std::vector<uint8_t> resp;
    return SendRecvCmd(canfd::OPEN_TACTILE, nullptr, 0, &resp) &&
           !resp.empty() && resp[0] == 1;
}

bool CANFDComm::CloseTactile() {
    std::vector<uint8_t> resp;
    return SendRecvCmd(canfd::CLOSE_TACTILE, nullptr, 0, &resp) &&
           !resp.empty() && resp[0] == 1;
}

bool CANFDComm::ZeroTactile() {
    std::vector<uint8_t> resp;
    return SendRecvCmd(canfd::ZERO_TACTILE, nullptr, 0, &resp) &&
           !resp.empty() && resp[0] == 1;
}

// ===== 数据回调 =====

void CANFDComm::SetJointsCallback(JointsCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    joints_cb_ = cb;
}

void CANFDComm::SetHandStateCallback(HandStateCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    hand_state_cb_ = cb;
}

void CANFDComm::SetTactileDataCallback(TactileDataCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    tactile_cb_ = cb;
}

// ===== 固件更新 =====

int CANFDComm::BootUpdate(const std::string& device_name,
                          uint16_t slave,
                          const std::string& filename,
                          std::function<void(int)> progress) {
    (void)device_name;
    (void)slave;
    (void)filename;
    (void)progress;
    LOG_WARNING("BootUpdate not supported on CANFD in Phase 1");
    return -1;
}

// ===== 内部协议方法 =====

bool CANFDComm::SendRecvCmd(canfd::FunctionCode fc,
                            const uint8_t* param,
                            uint8_t param_len,
                            std::vector<uint8_t>* response,
                            int timeout_ms) {
    if (!driver_ || !IsConnected()) return false;

    // 1. 登记等待槽位
    uint8_t fc_byte = static_cast<uint8_t>(fc);
    {
        std::lock_guard<std::mutex> lock(response_mutex_);
        response_slots_[fc_byte] = ResponseSlot{};
    }

    // 2. 构造完整数据（1 字节 FC + 全部参数，支持多帧）
    std::vector<uint8_t> full_data;
    full_data.reserve(1 + param_len);
    full_data.push_back(fc_byte);
    if (param && param_len > 0) {
        size_t offset = full_data.size();
        full_data.resize(offset + param_len);
        memcpy(full_data.data() + offset, param, param_len);
    }

    // 明确区分单帧/多帧发送
    bool sent = false;
    if (full_data.size() <= 64) {
        sent = SendSingleFrame(full_data.data(), static_cast<uint8_t>(full_data.size()));
    } else {
        sent = SendMultiFrame(full_data.data(), static_cast<uint8_t>(full_data.size()));
    }
    if (!sent) {
        std::lock_guard<std::mutex> lock(response_mutex_);
        response_slots_.erase(fc_byte);
        return false;
    }

    // 3. 阻塞等待本功能码的响应（支持多帧组包）
    {
        std::unique_lock<std::mutex> lock(response_mutex_);
        bool got_response = response_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
            [&] {
                auto it = response_slots_.find(fc_byte);
                return it == response_slots_.end() || it->second.ready;
            });

        auto it = response_slots_.find(fc_byte);
        if (!got_response || it == response_slots_.end() || !it->second.ready) {
            response_slots_.erase(fc_byte);
            return false;  // 超时或 Disconnect 清空
        }

        // 去掉首字节功能码，返回纯 payload
        *response = std::vector<uint8_t>(it->second.payload.begin() + 1,
                                         it->second.payload.end());
        response_slots_.erase(it);
        return true;
    }
}

bool CANFDComm::SendCmdOnly(canfd::FunctionCode fc,
                                      const uint8_t* param,
                                      uint8_t param_len) {
    if (!driver_ || !IsConnected()) return false;

    std::vector<uint8_t> full_data;
    full_data.reserve(1 + param_len);
    full_data.push_back(static_cast<uint8_t>(fc));
    if (param && param_len > 0) {
        size_t offset = full_data.size();
        full_data.resize(offset + param_len);
        memcpy(full_data.data() + offset, param, param_len);
    }

    if (full_data.size() <= 64) {
        return SendSingleFrame(full_data.data(), static_cast<uint8_t>(full_data.size()));
    } else {
        return SendMultiFrame(full_data.data(), static_cast<uint8_t>(full_data.size()));
    }
}

bool CANFDComm::SendSingleFrame(const uint8_t* data, uint8_t total_len) {
    if (!driver_) return false;

    canfd::Frame frame;
    frame.id = canfd::ArbitrationId(canfd::COMMAND, device_id_, 0, 1).raw;
    memcpy(frame.data, data, total_len);

    // 填充到 CANFD 合法长度
    const uint8_t valid_lens[] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};
    frame.len = 64;
    for (uint8_t vl : valid_lens) {
        if (vl >= total_len) {
            frame.len = vl;
            break;
        }
    }

    frame.is_fd = true;
    frame.is_extended = true;

    return driver_->Send(frame) == 0;
}

bool CANFDComm::SendMultiFrame(const uint8_t* data, uint8_t total_len) {
    if (!driver_) return false;

    uint8_t offset = 0;
    uint8_t frame_idx = 0;
    uint8_t total_frames = (total_len + 63) / 64;  // 向上取整

    while (offset < total_len) {
        uint8_t chunk = std::min<uint8_t>(total_len - offset, 64);

        canfd::Frame frame;
        frame.id = canfd::ArbitrationId(canfd::COMMAND, device_id_, frame_idx, total_frames).raw;
        memcpy(frame.data, data + offset, chunk);

        // 填充到 CANFD 合法长度
        if (frame_idx < total_frames - 1) {
            frame.len = 64;
        } else {
            const uint8_t valid_lens[] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};
            for (uint8_t vl : valid_lens) {
                if (vl >= chunk) {
                    frame.len = vl;
                    break;
                }
            }
        }

        frame.is_fd = true;
        frame.is_extended = true;

        if (driver_->Send(frame) != 0) {
            return false;
        }

        offset += chunk;
        frame_idx++;
    }
    return true;
}

// ===== 接收线程 =====

void CANFDComm::ReceiveThread() {
    while (rx_running_) {
        canfd::Frame frame;
        int result = driver_->Receive(frame, 100);  // 100ms timeout
        if (result != 0) continue;

        canfd::ArbitrationId arb(frame.id);

        // 只处理目标设备的帧
        if (arb.device_id() != device_id_ && device_id_!=0)
            continue;

        // 按 (device_id, FrameType) 分组组包，隔离 RESPONSE / ACTIVE_REPORT
        auto key = std::make_pair(device_id_, arb.frame_type());
        std::vector<uint8_t> payload;
        bool complete = false;
        {
            std::lock_guard<std::mutex> lock(asm_mutex_);
            complete = assemblers_[key].Feed(frame, &payload);
        }

        if (!complete) continue;

        if (arb.frame_type() == canfd::RESPONSE) {
            // 响应组包完成，按功能码投递到对应槽位
            std::lock_guard<std::mutex> lock(response_mutex_);
            if (!payload.empty()) {
                uint8_t fc = payload[0];
                auto it = response_slots_.find(fc);
                if (it != response_slots_.end()) {
                    it->second.payload = std::move(payload);
                    it->second.ready = true;
                    response_cv_.notify_all();
                }
            }
        } else if (arb.frame_type() == canfd::ACTIVE_REPORT) {
            // 主动上报组包完成，解析
            if (!payload.empty()) {
                ProcessActiveReport(payload, static_cast<canfd::FunctionCode>(payload[0]));
            }
        }
    }
}

void CANFDComm::ProcessActiveReport(const std::vector<uint8_t>& payload,
                                    canfd::FunctionCode fc) {
    switch (fc) {
        case canfd::REPORT_JOINTS:
            ParseJointStates(payload.data() + 1, payload.size() - 1);
            break;
        case canfd::REPORT_TACTILE:
            ParseTactileForce(payload.data() + 1, payload.size() - 1);
            break;
        case canfd::REPORT_ERROR:
            ParseHandError(payload.data() + 1, payload.size() - 1);
            break;
        default:
            break;
    }
}

bool CANFDComm::SubscribeActiveReport(canfd::FunctionCode fc, uint8_t period_ms) {
    uint8_t param[1] = {period_ms};
    return SendCmdOnly(fc, param, 1);
}

bool CANFDComm::UnsubscribeActiveReport(canfd::FunctionCode fc) {
    uint8_t param[1] = {0};
    return SendCmdOnly(fc, param, 1);
}

// ===== 数据解析 =====

void CANFDComm::ParseJointStates(const uint8_t* data, size_t len) {
    if (len < config_.valid_joints.size() * kCanfdJointDataSize) return;

    std::vector<Joint> joints;
    joints.reserve(config_.valid_joints.size());

    size_t offset = 0;
    for (const auto& joint_id : config_.valid_joints) {
        uint8_t joint_state = data[offset++];
        uint8_t joint_error = data[offset++];
        uint16_t raw_angle = data[offset] | (data[offset + 1] << 8);
        offset += 2;
        uint8_t joint_velocity = data[offset++];
        uint8_t joint_torque = data[offset++];

        Joint joint;
        joint.id = joint_id;
        joint.state = static_cast<State>(joint_state);
        joint.error = static_cast<ErrorCode>(joint_error);
        joint.angle = RawToAngle(raw_angle, joint_id);
        joint.velocity = static_cast<int8_t>(joint_velocity);
        joint.torque = static_cast<int8_t>(joint_torque);
        joints.push_back(joint);
    }

    std::lock_guard<std::mutex> lock(cb_mutex_);
    if (joints_cb_) joints_cb_(joints);
    // if (hand_state_cb_) hand_state_cb_(hand_state);
}

void CANFDComm::ParseTactileForce(const uint8_t* data, size_t len) {
    if (!config_.has_tactile) return;

    // 至少需要每个区域的合力数据（6 字节/区域）
    size_t min_size = config_.tactile_regions.size() * 6;
    if (len < min_size) return;

    TactileData tactile;
    size_t offset = 0;

    tactile.regions.reserve(config_.tactile_regions.size());
    for (size_t i = 0; i < config_.tactile_regions.size(); ++i) {
        const auto& rc = config_.tactile_regions[i];
        RegionTactile region;
        region.region_name = rc.name.c_str();

        int16_t raw_x = static_cast<int16_t>(data[offset] | (data[offset + 1] << 8));
        offset += 2;
        int16_t raw_y = static_cast<int16_t>(data[offset] | (data[offset + 1] << 8));
        offset += 2;
        int16_t raw_z = static_cast<int16_t>(data[offset] | (data[offset + 1] << 8));
        offset += 2;

        region.resultant_force.x = raw_x * 0.1f;
        region.resultant_force.y = raw_y * 0.1f;
        region.resultant_force.z = raw_z * 0.1f;

        int sample_size = rc.sensor_count * 3;
        if (rc.sensor_count > 0 && offset + sample_size <= len) {
            region.distributed_forces.reserve(rc.sensor_count);
            for (int j = 0; j < rc.sensor_count; ++j) {
                Force f;
                f.x = static_cast<float>(static_cast<int8_t>(data[offset])) * 0.1f;
                f.y = static_cast<float>(static_cast<int8_t>(data[offset + 1])) * 0.1f;
                f.z = static_cast<float>(static_cast<uint8_t>(data[offset + 2])) * 0.1f;
                offset += 3;
                region.distributed_forces.push_back(f);
            }
        }

        tactile.regions.push_back(std::move(region));
    }

    std::lock_guard<std::mutex> lock(cb_mutex_);
    if (tactile_cb_) tactile_cb_(tactile);
}

void CANFDComm::ParseHandError(const uint8_t* data, size_t len) {
    HandState hand_state;
    if (len >= 1) {
        hand_state.state = static_cast<State>(data[0]);
    }
    if (len >= 2) {
        hand_state.error = static_cast<ErrorCode>(data[1]);
    }
    if (len >= 4) {
        int16_t temp = static_cast<int16_t>(data[2] | (data[3] << 8));
        hand_state.temperature = temp;
    }
    std::lock_guard<std::mutex> lock(cb_mutex_);
    if (hand_state_cb_) hand_state_cb_(hand_state);
}

// ===== 角度转换 =====

float CANFDComm::RawToAngle(uint16_t raw, JointId id) {
    auto it = config_.joint_limits.find(id);
    if (it == config_.joint_limits.end()) return 0.0f;

    float min_angle = it->second.first;
    float max_angle = it->second.second;
    float angle_deg = min_angle + (raw / 1000.0f) * (max_angle - min_angle);
    return angle_deg;
}

uint16_t CANFDComm::AngleToRaw(float angle_rad, JointId id) {
    auto it = config_.joint_limits.find(id);
    if (it == config_.joint_limits.end()) return 0;

    float min_angle = it->second.first;
    float max_angle = it->second.second;
    float angle_deg = angle_rad ;

    if (angle_deg < min_angle) angle_deg = min_angle;
    if (angle_deg > max_angle) angle_deg = max_angle;

    uint16_t raw = static_cast<uint16_t>(((angle_deg - min_angle) / (max_angle - min_angle)) * 1000.0f);
    if (raw > 1000) raw = 1000;
    return raw;
}

} // namespace internal
} // namespace ghand
