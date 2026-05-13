#ifndef GHAND_INTERNAL_DEXHAND_CALLBACK_MANAGER_H_
#define GHAND_INTERNAL_DEXHAND_CALLBACK_MANAGER_H_

#include <chrono>
#include <functional>
#include <mutex>
#include <vector>
#include <cmath>

#include "ghand/types.h"

namespace ghand {
namespace internal {

/**
 * @brief DexHand回调管理器内部实现类
 *
 * 职责：
 * 1. 管理各类数据回调的注册
 * 2. 检测数据变化
 * 3. 在数据变化时触发相应的回调
 * 4. 保证数据新鲜度（100ms强制更新）
 * 5. 缓存最新数据，支持轮询访问
 */
class DexHandCallbackManager {
public:
    // 回调类型定义
    using JointsCallback = std::function<void(const std::vector<Joint>&)>;
    using HandStateCallback = std::function<void(const HandState&)>;
    using TactileDataCallback = std::function<void(const TactileData&)>;

    DexHandCallbackManager();
    ~DexHandCallbackManager() = default;

    // 回调注册方法
    void SetJointsCallback(JointsCallback callback);
    void SetHandStateCallback(HandStateCallback callback);
    void SetTactileDataCallback(TactileDataCallback callback);

    // 数据更新方法（由DexHand调用）
    void UpdateJoints(const std::vector<Joint>& joints);
    void UpdateTemperature(const HandState& temperature);
    void UpdateTactileData(const TactileData& data);

    // 轮询访问方法（线程安全）
    HandState GetHandData() const;
    std::vector<Joint> GetJointsData() const;
    TactileData GetTactileData() const;

private:
    // 变化检测方法
    bool HasJointDataChanged(const std::vector<Joint>& joints);
    bool HasTemperatureChanged(const HandState& temperature);

    // 回调成员变量
    JointsCallback joints_callback_;
    HandStateCallback hand_state_callback_;
    TactileDataCallback tactile_data_callback_;

    // 上次数据缓存（用于变化检测 + 轮询访问）
    mutable std::mutex data_mutex_;
    std::vector<Joint> last_joints_;
    HandState last_state_;
    TactileData last_tactile_data_;
    bool has_tactile_data_ = false;
    std::chrono::steady_clock::time_point last_joint_callback_time_;
    std::chrono::steady_clock::time_point last_temp_callback_time_;

    // 变化检测阈值常量
    static constexpr float JOINT_ANGLE_THRESHOLD = 1.0f;      // 1度
    static constexpr float TEMPERATURE_THRESHOLD = 1.0f;     // 1摄氏度
    static constexpr int DATA_FRESHNESS_MS = 100;            // 100ms数据新鲜度
};

}  // namespace internal
}  // namespace ghand

#endif  // GHAND_INTERNAL_DEXHAND_CALLBACK_MANAGER_H_
