#include "xiaoyao/dexhand_callback_manager.h"

#include <chrono>

namespace xiaoyao {

DexHandCallbackManager::DexHandCallbackManager() {
    // 初始化缓存数据
    last_joints_.clear();
    last_temperature_ = HandTemperature{State::STOPPED, ErrorCode::NORMAL, 0};
    last_joint_callback_time_ = std::chrono::steady_clock::now();
    last_temp_callback_time_ = std::chrono::steady_clock::now();
}

void DexHandCallbackManager::UpdateJoints(const std::vector<Joint>& joints) {
    if (HasJointDataChanged(joints)) {
        if (joints_callback_) {
            joints_callback_(joints);
        }
        last_joints_ = joints;
        last_joint_callback_time_ = std::chrono::steady_clock::now();
    }
}

void DexHandCallbackManager::UpdateTemperature(const HandTemperature& temperature) {
    if (HasTemperatureChanged(temperature)) {
        if (temperature_callback_) {
            temperature_callback_(temperature);
        }
        last_temperature_ = temperature;
        last_temp_callback_time_ = std::chrono::steady_clock::now();
    }
}

void DexHandCallbackManager::UpdateTactileData(const TactileData& data) {
    if (tactile_data_callback_) {
        tactile_data_callback_(data);
    }
}

bool DexHandCallbackManager::HasJointDataChanged(const std::vector<Joint>& joints) {
    // 首次更新
    if (last_joints_.empty()) {
        return true;
    }

    // 1. 状态或错误变化 → 立即触发（最高优先级）
    for (size_t i = 0; i < joints.size() && i < last_joints_.size(); i++) {
        if (joints[i].state != last_joints_[i].state ||
            joints[i].error != last_joints_[i].error) {
            return true;  // 立即触发，不管角度是否变化
        }
    }

    // 2. 角度变化触发（>1°）
    for (size_t i = 0; i < joints.size() && i < last_joints_.size(); i++) {
        float angle_diff = std::abs(joints[i].angle - last_joints_[i].angle);
        if (angle_diff > JOINT_ANGLE_THRESHOLD) {
            return true;
        }
    }

    // 3. 数据新鲜度检查（100ms未更新则强制发送）
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_joint_callback_time_);
    if (age.count() > DATA_FRESHNESS_MS) {
        return true;  // 强制发送，保证数据新鲜
    }

    return false;
}

bool DexHandCallbackManager::HasTemperatureChanged(const HandTemperature& temperature) {
    // 1. 状态或错误变化 → 立即触发
    if (temperature.state != last_temperature_.state ||
        temperature.error != last_temperature_.error) {
        return true;
    }

    // 2. 温度变化触发（>1°C）
    int temp_diff = std::abs(temperature.temperature - last_temperature_.temperature);
    if (temp_diff > TEMPERATURE_THRESHOLD) {
        return true;
    }

    // 3. 数据新鲜度检查（100ms）
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_temp_callback_time_);
    if (age.count() > DATA_FRESHNESS_MS) {
        return true;
    }

    return false;
}

}  // namespace xiaoyao
