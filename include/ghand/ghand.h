#ifndef GHAND_H_
#define GHAND_H_

#include "export.h"
#include "types.h"
#include "version.h"
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace ghand {

// 前向声明内部实现类
namespace internal {
class DexHand;
}  // namespace internal

// ========== 回调类型定义 ==========
using JointsCallback = std::function<void(const std::vector<Joint>&)>;
using HandStateCallback = std::function<void(const HandState&)>;
using TactileDataCallback = std::function<void(const TactileData&)>;

/**
 * @brief GHand DexHand 机械手控制接口
 *
 * 这是唯一的公共 API 类，提供对机械手的完整控制。
 */
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4251)
#endif
class GHAND_API DexHand {
public:
    static std::unique_ptr<DexHand> Create(ProductType product_type = ProductType::AUTO,
                                            CommType comm_type = CommType::ETHERCAT);
    ~DexHand();

    // 禁止拷贝
    DexHand(const DexHand&) = delete;
    DexHand& operator=(const DexHand&) = delete;

    // ========== 连接管理 ==========
    /**
     * @brief 自动连接设备
     * @return 成功返回 true
     */
    bool AutoConnect();

    /**
     * @brief 连接到指定设备
     * @param comm_type 通信类型
     * @param device_name 设备名称（"auto" 表示自动搜索）
     * @return 成功返回 true
     */
    bool Connect(const std::string& device_name = "auto");

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
    std::map<std::string, std::string> SearchAdapters();

    /**
     * @brief 获取手部类型
     */
    HandType GetHandType();

    /**
     * @brief 获取设备信息
     */
    DeviceInfo GetDeviceInfo();

    // ========== 运动控制 ==========
    /**
     * @brief 设置控制模式
     */
    void SetControlMode(ControlMode mode);

    /**
     * @brief 控制关节运动
     * @param joints 关节命令列表，角度单位为度（deg）
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

    // ========== 数据获取 ==========
    /**
     * @brief 获取手部状态（最新缓存数据，不触发硬件读取）
     */
    HandState GetHandData();

    /**
     * @brief 获取关节数据（最新缓存数据，不触发硬件读取）
     */
    std::vector<Joint> GetJointsData();

    /**
     * @brief 获取触觉数据（最新缓存数据，不触发硬件读取）
     */
    TactileData GetTactileData();

    // ========== 回调注册 ==========
    /**
     * @brief 注册关节数据回调
     * @param cb 回调函数
     */
    void SetJointsCallback(JointsCallback cb);

    /**
     * @brief 注册手部状态回调
     * @param cb 回调函数
     */
    void SetHandStateCallback(HandStateCallback cb);

    /**
     * @brief 注册触觉数据回调
     * @param cb 回调函数
     */
    void SetTactileDataCallback(TactileDataCallback cb);

    // Firmware update
    int BootUpdate(const std::string& ifname,
                   uint16_t slave,
                   const std::string& filename,
                   std::function<void(int)> progressCallback);

 private:
    DexHand(ProductType product_type, CommType comm_type);
    std::unique_ptr<internal::DexHand> impl_;
};

}  // namespace ghand

#ifdef _WIN32
#pragma warning(pop)
#endif

#endif  // GHAND_H_
