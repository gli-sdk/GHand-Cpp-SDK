#ifndef XIAOYAO_H_
#define XIAOYAO_H_

#include "types.h"
#include "version.h"
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace xiaoyao {

// ========== 回调类型定义 ==========
using JointsCallback = std::function<void(const std::vector<Joint>&)>;
using TemperatureCallback = std::function<void(const HandTemperature&)>;
using TactileDataCallback = std::function<void(const TactileData&)>;

/**
 * @brief Xiaoyao DexHand 机械手控制接口
 *
 * 这是唯一的公共 API 类，提供对机械手的完整控制。
 */
class DexHand {
public:
    explicit DexHand();
    ~DexHand();

    // 禁止拷贝
    DexHand(const DexHand&) = delete;
    DexHand& operator=(const DexHand&) = delete;

    // ========== 连接管理 ==========
    /**
     * @brief 自动连接设备
     * @param comm_type 通信类型（默认 ETHERCAT）
     * @return 成功返回 true
     */
    bool AutoConnect(CommType comm_type = CommType::ETHERCAT);

    /**
     * @brief 连接到指定设备
     * @param comm_type 通信类型
     * @param device_name 设备名称（"auto" 表示自动搜索）
     * @return 成功返回 true
     */
    bool Connect(CommType comm_type = CommType::ETHERCAT,
                const std::string& device_name = "auto");

    /**
     * @brief 断开连接
     */
    bool Disconnect();

    /**
     * @brief 检查是否已连接
     */
    bool IsConnected() const;

    // ========== 设备信息 ==========
    /**
     * @brief 搜索可用的通信适配器
     * @return 适配器名称映射表
     */
    std::map<std::string, std::string> SearchAdapters() const;

    /**
     * @brief 获取手部类型
     */
    HandType GetHandType() const;

    /**
     * @brief 获取设备信息
     */
    DeviceInfo GetDeviceInfo() const;

    // ========== 运动控制 ==========
    /**
     * @brief 设置控制模式
     */
    void SetControlMode(ControlMode mode);

    /**
     * @brief 控制关节运动
     * @param joints 关节命令列表
     * @return 成功返回 true
     */
    bool MoveJoints(const std::vector<JointCommand>& joints);

    /**
     * @brief 立即停止所有运动
     */
    void Stop();

    /**
     * @brief 清除故障状态
     */
    bool ClearFault();

    /**
     * @brief 初始化关节
     */
    bool InitJoint();

    // ========== 触觉传感器 ==========
    /**
     * @brief 打开触觉传感器
     */
    bool OpenTactile();

    /**
     * @brief 关闭触觉传感器
     */
    bool CloseTactile();

    /**
     * @brief 触觉传感器清零
     */
    bool ZeroTactile();

    // ========== 回调注册 ==========
    /**
     * @brief 注册关节数据回调
     * @param cb 回调函数
     */
    void SetJointsCallback(JointsCallback cb);

    /**
     * @brief 注册温度数据回调
     * @param cb 回调函数
     */
    void SetTemperatureCallback(TemperatureCallback cb);

    /**
     * @brief 注册触觉数据回调
     * @param cb 回调函数
     */
    void SetTactileDataCallback(TactileDataCallback cb);

private:
    class Impl;  // Pimpl 模式
    std::unique_ptr<Impl> impl_;
};

}  // namespace xiaoyao

#endif  // XIAOYAO_H_
