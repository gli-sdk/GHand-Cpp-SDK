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

class DexHand {
public:
    explicit DexHand();
    ~DexHand();

    // Connection management
    bool AutoConnect(CommType comm_type = CommType::ETHERCAT);
    bool Connect(CommType comm_type = CommType::ETHERCAT,
                const std::string& device_name = "auto");
    bool Disconnect();
    bool IsConnected() const;

    // Device information
    std::map<std::string, std::string> SearchAdapters() const;
    HandType GetHandType() const;
    DeviceInfo GetDeviceInfo() const;

    // Control operations
    void SetControlMode(ControlMode mode);
    bool MoveJoints(const std::vector<JointCommand>& joints);
    void Stop();
    bool ClearFault();
    bool InitJoint();

    // Tactile sensor control
    bool OpenTactile();
    bool CloseTactile();
    bool ZeroTactile();

    // Callback registration
    void SetJointsCallback(DexHandCallbackManager::JointsCallback cb) {
        callback_manager_.SetJointsCallback(cb);
    }

    void SetTemperatureCallback(DexHandCallbackManager::TemperatureCallback cb) {
        callback_manager_.SetTemperatureCallback(cb);
    }

    void SetTactileDataCallback(DexHandCallbackManager::TactileDataCallback cb) {
        callback_manager_.SetTactileDataCallback(cb);
    }

private:
    std::unique_ptr<EtherCATComm> ethercat_comm_;
    bool ConnectToDevice(CommType comm_type, const std::string& device_name);

    mutable HandType hand_type_ = HandType::NONE;
    mutable DeviceInfo device_info_;
    ControlMode control_mode_ = ControlMode::POSITION;

    DexHandCallbackManager callback_manager_;
    void OnRawDataReceived(const uint8_t* data, size_t size);
};

}  // namespace xiaoyao

#endif  // XIAOYAO_DEXHAND_H_
