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

void GHandCallbackManager::SetJointsCallback(JointsCallback callback) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  joints_callback_ = callback;
}

void GHandCallbackManager::SetHandStateCallback(HandStateCallback callback) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  hand_state_callback_ = callback;
}

void GHandCallbackManager::SetTactileDataCallback(
    TactileDataCallback callback) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  tactile_data_callback_ = callback;
}

void GHandCallbackManager::SetSelectedSlave(uint16_t slave) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  selected_slave_ = slave;
  SlaveCache& cache = caches_[slave];
  cache.delivered_joints.clear();
  cache.last_temp_callback_time =
      std::chrono::steady_clock::time_point{};
}

void GHandCallbackManager::UpdateJoints(uint16_t slave,
                                        const std::vector<Joint>& joints) {
  JointsCallback callback;
  std::vector<Joint> payload;
  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    SlaveCache& cache = caches_[slave];
    // Cache every slave so switching hands exposes its latest feedback.
    cache.joints = joints;
    if (slave == selected_slave_) {
      // Change detection is based on the "last actually delivered value",
      // not on the previous polled frame.
      if (HasJointDataChanged(cache, joints)) {
        callback = joints_callback_;
        cache.delivered_joints =
            joints;  // Advance the baseline only when delivering
        cache.last_joint_callback_time = std::chrono::steady_clock::now();
        payload = joints;  // Always deliver the full joint set, not a subset
      }
    }
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
    if (slave == selected_slave_) {
      // Change detection runs against the previous cached state, so check
      // before overwriting it.
      if (HasTemperatureChanged(cache, temperature)) {
        callback = hand_state_callback_;
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
    if (slave == selected_slave_) {
      callback = tactile_data_callback_;
    }
  }
  if (callback) {
    callback(data);
  }
}

// Polling access

HandState GHandCallbackManager::GetHandData() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  const auto it = caches_.find(selected_slave_);
  if (it == caches_.end()) {
    return HandState{State::STOPPED, ErrorCode::NORMAL, 0};
  }
  return it->second.state;
}

std::vector<Joint> GHandCallbackManager::GetJointsData() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  const auto it = caches_.find(selected_slave_);
  if (it == caches_.end()) {
    return std::vector<Joint>{};
  }
  return it->second.joints;
}

TactileData GHandCallbackManager::GetTactileData() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  const auto it = caches_.find(selected_slave_);
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
