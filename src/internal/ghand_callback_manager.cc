// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include "ghand_callback_manager.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "ghand/logging.h"
#include "logging_macros.h"

namespace ghand {
namespace internal {

namespace {
const Joint* FindJointById(const std::vector<Joint>& joints, JointId id) {
  for (const auto& j : joints) {
    if (j.id == id) return &j;
  }
  return nullptr;
}
}  // namespace

void GHandCallbackManager::SetJointsCallback(uint16_t slave,
                                             JointsCallback callback) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  if (callback) {
    joints_callbacks_[slave] = callback;
  } else {
    joints_callbacks_.erase(slave);
  }
}

void GHandCallbackManager::SetHandStateCallback(uint16_t slave,
                                                HandStateCallback callback) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  if (callback) {
    hand_state_callbacks_[slave] = callback;
  } else {
    hand_state_callbacks_.erase(slave);
  }
}

void GHandCallbackManager::SetTactileDataCallback(
    uint16_t slave, TactileDataCallback callback) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  if (callback) {
    tactile_data_callbacks_[slave] = callback;
  } else {
    tactile_data_callbacks_.erase(slave);
  }
}

void GHandCallbackManager::UpdateJoints(uint16_t slave,
                                        const std::vector<Joint>& joints) {
  JointsCallback callback;
  std::vector<Joint> payload;
  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    SlaveCache& cache = caches_[slave];
    const auto it = joints_callbacks_.find(slave);
    if (it != joints_callbacks_.end()) {
      // Change detection is based on the "last actually delivered value", not
      // on the previous polled frame.
      if (HasJointDataChanged(cache, joints)) {
        callback = it->second;
        cache.delivered_joints =
            joints;  // Advance the baseline only when delivering
        cache.last_joint_callback_time = std::chrono::steady_clock::now();
        payload = joints;  // Always deliver the full joint set, not a subset
      }
    }
    cache.joints = joints;
  }

  if (callback) {
    callback(payload);
  }
}

void GHandCallbackManager::UpdateTemperature(uint16_t slave,
                                             const HandState& temperature) {
  HandStateCallback callback;
  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    SlaveCache& cache = caches_[slave];
    const auto it = hand_state_callbacks_.find(slave);
    if (it != hand_state_callbacks_.end()) {
      // Change detection runs against the previous cached state, so check
      // before overwriting it.
      if (HasTemperatureChanged(cache, temperature)) {
        callback = it->second;
        cache.last_temp_callback_time = std::chrono::steady_clock::now();
      }
    }
    cache.state = temperature;
  }
  if (callback) {
    callback(temperature);
  }
}

void GHandCallbackManager::UpdateTactileData(uint16_t slave,
                                             const TactileData& data) {
  TactileDataCallback callback;
  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    SlaveCache& cache = caches_[slave];
    cache.tactile = data;
    const auto it = tactile_data_callbacks_.find(slave);
    if (it != tactile_data_callbacks_.end()) {
      callback = it->second;
    }
  }
  if (callback) {
    callback(data);
  }
}

// Polling access

HandState GHandCallbackManager::GetHandData(uint16_t slave) const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  const auto it = caches_.find(slave);
  if (it == caches_.end()) {
    return HandState{State::STOPPED, ErrorCode::NORMAL, 0};
  }
  return it->second.state;
}

std::vector<Joint> GHandCallbackManager::GetJointsData(uint16_t slave) const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  const auto it = caches_.find(slave);
  if (it == caches_.end()) {
    return std::vector<Joint>{};
  }
  return it->second.joints;
}

TactileData GHandCallbackManager::GetTactileData(uint16_t slave) const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  const auto it = caches_.find(slave);
  if (it == caches_.end()) {
    return TactileData{};
  }
  return it->second.tactile;
}

bool GHandCallbackManager::HasJointDataChanged(
    const SlaveCache& cache, const std::vector<Joint>& joints) {
  // First delivery
  if (cache.delivered_joints.empty()) {
    return true;
  }
  // 1. State/error changes -> deliver immediately
  for (const auto& joint : joints) {
    const Joint* previous = FindJointById(cache.delivered_joints, joint.id);
    if (previous == nullptr) {
      return true;
    }
    if (joint.state != previous->state || joint.error != previous->error) {
      return true;
    }
  }

  // 2. Accumulated angle change relative to the "last delivered value"
  //    exceeds the threshold.
  for (const auto& joint : joints) {
    const Joint* previous = FindJointById(cache.delivered_joints, joint.id);
    if (previous != nullptr &&
        std::abs(joint.angle - previous->angle) > kJointAngleThreshold) {
      return true;
    }
  }
  // 3. Freshness fallback: force delivery if no data has been delivered
  //    for more than 100 ms.
  auto now = std::chrono::steady_clock::now();
  auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - cache.last_joint_callback_time);
  if (age.count() > kDataFreshnessMs) {
    return true;
  }
  return false;
}

bool GHandCallbackManager::HasTemperatureChanged(
    const SlaveCache& cache, const HandState& temperature) {
  // 1. State or error changes trigger immediately
  if (temperature.state != cache.state.state ||
      temperature.error != cache.state.error) {
    GHAND_LOG_DEBUG(
        "Hand state change detected: " << ghand::ToString(temperature.state));
    return true;
  }

  // 2. Temperature change triggers (>1degC)
  int temp_diff = std::abs(temperature.temperature - cache.state.temperature);
  if (temp_diff > kTemperatureThreshold) {
    return true;
  }

  // 3. Data freshness check (100ms)
  auto now = std::chrono::steady_clock::now();
  auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - cache.last_temp_callback_time);
  if (age.count() > kDataFreshnessMs) {
    return true;
  }

  return false;
}

}  // namespace internal
}  // namespace ghand
