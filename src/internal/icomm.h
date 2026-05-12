#ifndef SRC_INTERNAL_ICOMM_H_
#define SRC_INTERNAL_ICOMM_H_

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "ghand/types.h"

namespace xiaoyao {
namespace internal {

using JointsCallback = std::function<void(const std::vector<Joint>&)>;
using HandStateCallback = std::function<void(const HandState&)>;
using TactileDataCallback = std::function<void(const TactileData&)>;

/**
 * @brief 通信抽象接口
 *
 * 为 EtherCAT/CANFD/RS485 提供统一的业务级通信接口。
 * 实现类负责处理底层协议差异，向上层提供标准化的设备操作。
 */
class IComm {
public:
    virtual ~IComm() = default;

    // ===== 连接管理 =====
    virtual int Connect(const std::string& device_name) = 0;
    virtual int Disconnect() = 0;
    virtual bool IsConnected() const = 0;
    virtual std::map<std::string, std::string> SearchAdapters() = 0;

    // ===== 设备信息 =====
    virtual DeviceInfo GetDeviceInfo() = 0;
    virtual HandType GetHandType() = 0;

    // ===== 运动控制 =====
    virtual bool MoveJoints(const std::vector<JointCommand>& joints,
                            ControlMode mode) = 0;
    virtual void Stop() = 0;

    // ===== 系统操作 =====
    virtual bool ClearFault() = 0;
    virtual bool InitJoint() = 0;

    // ===== 触觉传感器 =====
    virtual bool OpenTactile() = 0;
    virtual bool CloseTactile() = 0;
    virtual bool ZeroTactile() = 0;

    // ===== 数据回调 =====
    virtual void SetJointsCallback(JointsCallback cb) = 0;
    virtual void SetHandStateCallback(HandStateCallback cb) = 0;
    virtual void SetTactileDataCallback(TactileDataCallback cb) = 0;

    // ===== 固件更新 =====
    virtual int BootUpdate(const std::string& device_name,
                           uint16_t slave,
                           const std::string& filename,
                           std::function<void(int)> progress) = 0;
};

} // namespace internal
} // namespace xiaoyao

#endif // SRC_INTERNAL_ICOMM_H_
