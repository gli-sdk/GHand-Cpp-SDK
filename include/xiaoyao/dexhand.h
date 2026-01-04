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

struct HandTemperature {
    uint8_t state;
    uint8_t error;
    int16_t temperature;
};

enum CommType { COMM_ETHERCAT, COMM_CANFD, COMM_RS485 };
enum ConnectState { DISCONNECT, CONNECT };
enum OperationMode { MODE_NONE, MODE_INIT, MODE_NORMAL, MODE_BOOT, MODE_ERROR };

class DexHand {
   public:
    explicit DexHand();
    ~DexHand();

    HandType hand_type_ = HandType::NONE;
    ConnectState connect_state_ = DISCONNECT;
    OperationMode operation_mode_ = MODE_NONE;
    map<string, string> adapter_names_;
    DeviceInfo device_info_;
    int is_move_joints_ = 0;  // 1:点击开始；2:点击停止
    std::vector<Joint> joints_;
    HandTemperature hand_temperature_;

    int Open(CommType comm_type = COMM_ETHERCAT, std::string device_name = "auto");
    int Close();
    void ListAdapters();
    int MoveJoints();
    int GetJoints();
    void GetHandType(std::uint16_t slave);
    void GetDeviceInfo(std::uint16_t slave);
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