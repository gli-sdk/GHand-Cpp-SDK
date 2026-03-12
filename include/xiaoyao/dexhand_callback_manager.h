#ifndef XIAOYAO_DEXHAND_CALLBACK_MANAGER_H_
#define XIAOYAO_DEXHAND_CALLBACK_MANAGER_H_

#include <chrono>
#include <functional>
#include <vector>
#include <cmath>

#include "types.h"

namespace xiaoyao {

/**
 * @brief DexHand回调管理器
 *
 * 职责：
 * 1. 管理各类数据回调的注册
 * 2. 检测数据变化
 * 3. 在数据变化时触发相应的回调
 * 4. 保证数据新鲜度（100ms强制更新）
 */
class DexHandCallbackManager {
public:
    // 回调类型定义
    using JointsCallback = std::function<void(const std::vector<Joint>&)>;
    using TemperatureCallback = std::function<void(const HandTemperature&)>;
    using TactileDataCallback = std::function<void(const TactileData&)>;

    DexHandCallbackManager();
    ~DexHandCallbackManager() = default;

    // 回调注册方法
    /**
     * @brief 注册关节数据回调函数
     * @param callback 关节数据回调函数
     */
    void SetJointsCallback(JointsCallback callback) { joints_callback_ = callback; }

    /**
     * @brief 注册温度数据回调函数
     * @param callback 温度数据回调函数
     */
    void SetTemperatureCallback(TemperatureCallback callback) { temperature_callback_ = callback; }

    /**
     * @brief 注册触觉数据回调函数
     * @param callback 触觉数据回调函数，接收合力或分布力数据
     */
    void SetTactileDataCallback(TactileDataCallback callback) { tactile_data_callback_ = callback; }

    // 数据更新方法（由DexHand调用）
    /**
     * @brief 更新关节数据并触发回调
     * @param joints 关节数据向量
     */
    void UpdateJoints(const std::vector<Joint>& joints);

    /**
     * @brief 更新温度数据并触发回调
     * @param temperature 温度数据
     */
    void UpdateTemperature(const HandTemperature& temperature);

    /**
     * @brief 更新触觉数据并触发回调
     * @param data 触觉数据（合力或分布力）
     */
    void UpdateTactileData(const TactileData& data);

private:
    // 变化检测方法
    bool HasJointDataChanged(const std::vector<Joint>& joints);
    bool HasTemperatureChanged(const HandTemperature& temperature);

    // 回调成员变量
    JointsCallback joints_callback_;
    TemperatureCallback temperature_callback_;
    TactileDataCallback tactile_data_callback_;

    // 上次数据缓存（用于变化检测）
    std::vector<Joint> last_joints_;
    HandTemperature last_temperature_;
    std::chrono::steady_clock::time_point last_joint_callback_time_;
    std::chrono::steady_clock::time_point last_temp_callback_time_;

    // 变化检测阈值
    static constexpr float JOINT_ANGLE_THRESHOLD = 1.0f;      // 1度
    static constexpr float TEMPERATURE_THRESHOLD = 1.0f;     // 1摄氏度
    static constexpr int DATA_FRESHNESS_MS = 100;            // 100ms数据新鲜度
};

}  // namespace xiaoyao

#endif  // XIAOYAO_DEXHAND_CALLBACK_MANAGER_H_
