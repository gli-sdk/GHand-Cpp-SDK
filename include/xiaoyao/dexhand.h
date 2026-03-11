#ifndef XIAOYAO_DEXHAND_H_
#define XIAOYAO_DEXHAND_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ethercat_comm.h"
#include "finger.h"
#include "log_file.h"

enum HandType { NONE, LEFT, RIGHT, NUM_HANDS };

struct DeviceInfo {
    std::string device_name;
    std::string hardware_version;
    std::string software_version;
    std::string serial_number;
};

enum JointId {
    THUMB_DIP,
    THUMB_PIP,
    THUMB_MCP,
    THUMB_SWING,
    THUMB_ROTATION,
    FF_DIP,
    FF_PIP,
    FF_MCP,
    FF_SWING,
    MF_DIP,
    MF_PIP,
    MF_MCP,
    RF_DIP,
    RF_PIP,
    RF_MCP,
    LF_DIP,
    LF_PIP,
    LF_MCP,
    NUM_JOINTS
};

struct MotionParam {
    float angle;
    uint8_t velocity;
    uint8_t torque;
};

struct JointState {
    uint8_t state;
    uint8_t error;
    float angle;
    uint8_t velocity;
    uint8_t torque;
};

struct Joint {
    JointId id;
    MotionParam target;
    JointState state;
};

// 关节命令结构体，用于参数化关节控制
struct JointCommand {
    JointId id;              // 关节标识符
    MotionParam target;      // 目标参数（angle, velocity, torque）
};

struct HandTemperature {
    uint8_t state;
    uint8_t error;
    int16_t temperature;
};

enum CommType { COMM_ETHERCAT, COMM_CANFD, COMM_RS485 };
enum ConnectState { DISCONNECT, CONNECT };
enum OperationMode { MODE_NONE, MODE_INIT, MODE_NORMAL, MODE_BOOT, MODE_ERROR };

// 参数化关节控制模式
enum class ControlMode : uint8_t {
    POSITION = 0,  // 位置控制模式
    TORQUE = 1,    // 力矩控制模式
    SPEED = 2      // 速度控制模式
};

class DexHand {
   public:
    explicit DexHand();
    ~DexHand();

    OperationMode operation_mode_ = MODE_NONE;
    std::vector<Joint> joints_;
    HandTemperature hand_temperature_;

    int Open(CommType comm_type = COMM_ETHERCAT, std::string device_name = "auto");
    int Close();
    map<string, string> ListAdapters();

    // 参数化关节控制API
    bool MoveJoints(const std::vector<JointCommand>& commands);
    void SetControlMode(ControlMode mode);
    void Stop();
    int GetJoints();
    HandType GetHandType();
    DeviceInfo GetDeviceInfo();
    bool IsConnected() const;
    int ReleaseProtection();
    int InitJoint();
    // int ResetTactile();
    int BootUpdate(char* ifname, uint16_t slave, char* filename,
                   std::function<void(int)> progressCallback);

    void InitializeAllJoints();
    // 触觉
    bool OpenTactile();
    bool CloseTactile();
    bool ResetTactile();
    // 归零：单个手指和整手
    bool ResetToZero();
    // 获取单个手指或整手合力，返回state和error
    int GetResultantForce(FingerType finger_type, std::vector<Force>* resultant_forces);
    // 获取单个手指分布力
    int GetSampleForce(FingerType finger_type, std::vector<Force>* sample_forces);

   private:
    std::unique_ptr<EtherCATComm> ethercat_comm_;
    int AutoConnectDevices();
    int ConnectDevice(std::string device_name);

    HandType hand_type_ = HandType::NONE;
    DeviceInfo device_info_;
    ConnectState connect_state_ = DISCONNECT;
    ControlMode control_mode_ = ControlMode::POSITION;  // 当前控制模式

   private:
    Thumb thumb_;
    IndexFinger index_finger_;
    MiddleFinger middle_finger_;
    RingFinger ring_finger_;
    LittleFinger little_finger_;
    Thumb thumb() { return thumb_; }
    IndexFinger index_finger() { return index_finger_; }
    MiddleFinger middle_finger() { return middle_finger_; }
    RingFinger ring_finger() { return ring_finger_; }
    LittleFinger little_finger() { return little_finger_; }

   private:
    LogFile* log_file_;  // 日志文件对象

    //    private:
    //     std::mutex state_mutex_;
    void OnSlaveStateUpdate(int state) {
        // std::lock_guard<std::mutex> lock(state_mutex_);
        // log_file_->WriteLog("Slave state update: " + std::to_string(state));
        if (state == 8) {
            connect_state_ = CONNECT;
            operation_mode_ = MODE_NORMAL;
        } else if (state == 3) {
            connect_state_ = CONNECT;
            operation_mode_ = MODE_BOOT;
        } else if (state > 0) {
            connect_state_ = CONNECT;
            operation_mode_ = MODE_INIT;
        } else {
            connect_state_ = DISCONNECT;
            operation_mode_ = MODE_NONE;
            // log_file_->WriteLog("Disconnect");
        }
    }
};
#endif