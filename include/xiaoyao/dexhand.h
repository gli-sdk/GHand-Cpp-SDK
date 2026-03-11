#ifndef XIAOYAO_DEXHAND_H_
#define XIAOYAO_DEXHAND_H_

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "dexhand_callback_manager.h"
#include "ethercat_comm.h"
#include "finger.h"
#include "log_file.h"
#include "types.h"

enum HandType { NONE, LEFT, RIGHT, NUM_HANDS };

struct DeviceInfo {
    std::string device_name;
    std::string hardware_version;
    std::string software_version;
    std::string serial_number;
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

    // 回调注册方法（委托给回调管理器）
    void SetJointsCallback(DexHandCallbackManager::JointsCallback cb) {
        callback_manager_.SetJointsCallback(cb);
    }
    void SetTemperatureCallback(DexHandCallbackManager::TemperatureCallback cb) {
        callback_manager_.SetTemperatureCallback(cb);
    }
    void SetForceCallback(DexHandCallbackManager::ForceCallback cb) {
        callback_manager_.SetForceCallback(cb);
    }

    OperationMode operation_mode_ = MODE_NONE;

    int Open(CommType comm_type = COMM_ETHERCAT, std::string device_name = "auto");
    int Close();
    map<string, string> ListAdapters();

    // 参数化关节控制API
    bool MoveJoints(const std::vector<JointCommand>& commands);
    void SetControlMode(ControlMode mode);
    void Stop();
    HandType GetHandType();
    DeviceInfo GetDeviceInfo();
    bool IsConnected() const;
    int ReleaseProtection();
    int InitJoint();
    // int ResetTactile();
    int BootUpdate(char* ifname, uint16_t slave, char* filename,
                   std::function<void(int)> progressCallback);

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

    // 回调管理器（负责数据变化检测和回调触发）
    DexHandCallbackManager callback_manager_;

    // PDO数据回调处理方法
    void OnRawDataReceived(const uint8_t* data, size_t size);

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