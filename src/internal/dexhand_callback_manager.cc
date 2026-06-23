#include "dexhand_callback_manager.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "ghand/logging.h"
#include "logging_macros.h"

namespace ghand {
namespace internal {

DexHandCallbackManager::DexHandCallbackManager() {
  // Initialize cached data
  last_joints_.clear();
  last_state_ = HandState{State::STOPPED, ErrorCode::NORMAL, 0};
  last_joint_callback_time_ = std::chrono::steady_clock::now();
  last_temp_callback_time_ = std::chrono::steady_clock::now();
}

void DexHandCallbackManager::SetJointsCallback(JointsCallback callback) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  joints_callback_ = callback;
}

void DexHandCallbackManager::SetHandStateCallback(HandStateCallback callback) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  hand_state_callback_ = callback;
}

void DexHandCallbackManager::SetTactileDataCallback(
    TactileDataCallback callback) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  tactile_data_callback_ = callback;
}

void DexHandCallbackManager::UpdateJoints(const std::vector<Joint>& joints) {
  JointsCallback callback;
  std::vector<Joint> changed_joints;
  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (last_joints_.empty()) {
      changed_joints = joints;
    } else {
      for (const auto& joint : joints) {
        const Joint* previous = nullptr;
        for (const auto& cached : last_joints_) {
          if (cached.id == joint.id) {
            previous = &cached;
            break;
          }
        }
        if (previous == nullptr || joint.state != previous->state ||
            joint.error != previous->error ||
            std::abs(joint.angle - previous->angle) >
                kJointAngleThreshold) {
          changed_joints.push_back(joint);
        }
      }
    }
    last_joints_ = joints;
    if (!changed_joints.empty()) {
      callback = joints_callback_;
      last_joint_callback_time_ = std::chrono::steady_clock::now();
    }
  }
  if (callback) {
    callback(changed_joints);
  }
}

void DexHandCallbackManager::UpdateTemperature(const HandState& temperature) {
  HandStateCallback callback;
  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (HasTemperatureChanged(temperature)) {
      callback = hand_state_callback_;
      last_temp_callback_time_ = std::chrono::steady_clock::now();
    }
    last_state_ = temperature;
  }
  if (callback) {
    callback(temperature);
  }
}

void DexHandCallbackManager::UpdateTactileData(const TactileData& data) {
  TactileDataCallback callback;
  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    last_tactile_data_ = data;
    has_tactile_data_ = true;
    callback = tactile_data_callback_;
  }
  if (callback) {
    callback(data);
  }
}

// Polling access

HandState DexHandCallbackManager::GetHandData() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  return last_state_;
}

std::vector<Joint> DexHandCallbackManager::GetJointsData() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  return last_joints_;
}

TactileData DexHandCallbackManager::GetTactileData() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  return last_tactile_data_;
}

bool DexHandCallbackManager::HasJointDataChanged(
    const std::vector<Joint>& joints) {
  // First update
  if (last_joints_.empty()) {
    return true;
  }

  // 1. State or error changes → trigger immediately (highest priority)
  for (size_t i = 0; i < joints.size() && i < last_joints_.size(); i++) {
    if (joints[i].state != last_joints_[i].state ||
        joints[i].error != last_joints_[i].error) {
      return true;  // Trigger immediately regardless of angle changes
    }
  }

  // 2. Angle change triggers (>1°)
  for (size_t i = 0; i < joints.size() && i < last_joints_.size(); i++) {
    float angle_diff = std::abs(joints[i].angle - last_joints_[i].angle);
    if (angle_diff > kJointAngleThreshold) {
      return true;
    }
  }

  // 3. Data freshness check (force send if not updated for 100ms)
  auto now = std::chrono::steady_clock::now();
  auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - last_joint_callback_time_);
  if (age.count() > kDataFreshnessMs) {
    return true;  // Force send to ensure data freshness
  }

  return false;
}

bool DexHandCallbackManager::HasTemperatureChanged(
    const HandState& temperature) {
  // 1. State or error changes → trigger immediately
  if (temperature.state != last_state_.state ||
      temperature.error != last_state_.error) {
    GHAND_LOG_DEBUG(
        "Hand state change detected: " << ghand::ToString(temperature.state));
    return true;
  }

  // 2. Temperature change triggers (>1°C)
  int temp_diff = std::abs(temperature.temperature - last_state_.temperature);
  if (temp_diff > kTemperatureThreshold) {
    return true;
  }

  // 3. Data freshness check (100ms)
  auto now = std::chrono::steady_clock::now();
  auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - last_temp_callback_time_);
  if (age.count() > kDataFreshnessMs) {
    return true;
  }

  return false;
}

}  // namespace internal
}  // namespace ghand
