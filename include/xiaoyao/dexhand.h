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
#include "types.h"

namespace xiaoyao {

struct DeviceInfo {
    std::string device_name;
    std::string hardware_version;
    std::string software_version;
    std::string serial_number;
};

enum CommType { COMM_ETHERCAT, COMM_CANFD, COMM_RS485 };

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
    
    void SetTactileDataCallback(DexHandCallbackManager::TactileDataCallback cb) {
        callback_manager_.SetTactileDataCallback(cb);
    }

    int Open(CommType comm_type = COMM_ETHERCAT, std::string device_name = "auto");
    int Close();
    map<string, string> SearchAdapters();

    // 参数化关节控制API
    bool MoveJoints(const std::vector<JointCommand>& commands);
    void SetControlMode(ControlMode mode);
    void Stop();
    HandType GetHandType();
    DeviceInfo GetDeviceInfo();
    bool IsConnected() const;
    int ReleaseProtection();
    int InitJoint();
    int BootUpdate(char* ifname, uint16_t slave, char* filename,
                   std::function<void(int)> progressCallback);

    // 触觉
    bool OpenTactile();
    bool CloseTactile();
    // 归零：单个手指和整手
    bool ResetToZero();

   private:
    std::unique_ptr<EtherCATComm> ethercat_comm_;
    int AutoConnectDevices();
    int ConnectDevice(std::string device_name);

    HandType hand_type_ = HandType::NONE;
    DeviceInfo device_info_;
    ControlMode control_mode_ = ControlMode::POSITION;  // 当前控制模式

    // 回调管理器（负责数据变化检测和回调触发）
    DexHandCallbackManager callback_manager_;

    // PDO数据回调处理方法
    void OnRawDataReceived(const uint8_t* data, size_t size);
};

}  // namespace xiaoyao

#endif  // XIAOYAO_DEXHAND_H_