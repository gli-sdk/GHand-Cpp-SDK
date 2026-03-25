#ifndef SRC_INTERNAL_DEXHAND_H_
#define SRC_INTERNAL_DEXHAND_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "xiaoyao/types.h"

namespace xiaoyao {
namespace internal {

class EtherCATComm;
class DexHandCallbackManager;

/**
 * @brief DexHand内部实现类
 *
 * 该类包含DexHand的所有实现细节，通过Pimpl模式与公共API分离。
 * 用户代码不应直接访问此类，它仅用于SDK内部实现。
 */
class DexHand {
public:
    DexHand();
    ~DexHand();

    // 连接管理
    bool AutoConnect(CommType comm_type);
    bool Connect(CommType comm_type, const std::string& device_name);
    bool Disconnect();
    bool IsConnected() const;

    // 设备信息
    std::map<std::string, std::string> SearchAdapters() const;
    HandType GetHandType() const;
    DeviceInfo GetDeviceInfo() const;

    // 控制操作
    void SetControlMode(ControlMode mode);
    bool MoveJoints(const std::vector<JointCommand>& joints);
    void Stop();
    bool ClearFault();
    bool InitJoint();

    // 触觉传感器控制
    bool OpenTactile();
    bool CloseTactile();
    bool ZeroTactile();

    // 回调注册
    void SetJointsCallback(std::function<void(const std::vector<Joint>&)> cb);
    void SetHandStateCallback(std::function<void(const HandState&)> cb);
    void SetTactileDataCallback(std::function<void(const TactileData&)> cb);
    // Firmware update
    int BootUpdate(const std::string& ifname,
                   uint16_t slave,
                   const std::string& filename,
                   std::function<void(int)> progressCallback);

 private:
    bool ConnectToDevice(CommType comm_type, const std::string& device_name);
    void OnRawDataReceived(const uint8_t* data, size_t size);
    void ClampJointAngle(JointCommand& joint);
    void ClampJointVelocity(JointCommand& joint);
    void ClampJointTorque(JointCommand& joint);

    std::unique_ptr<EtherCATComm> ethercat_comm_;
    std::unique_ptr<DexHandCallbackManager> callback_manager_;

    mutable HandType hand_type_;
    mutable DeviceInfo device_info_;
    ControlMode control_mode_;

    // 关节限制静态表 (仅包含可控关节)
    static const std::map<JointId, std::pair<float, float>> kJointLimits_;
};

}  // namespace internal
}  // namespace xiaoyao

#endif  // SRC_INTERNAL_DEXHAND_H_
