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

GHandCallbackManager::GHandCallbackManager() {
  // Initialize cached data
  last_joints_.clear();
  last_state_ = HandState{State::STOPPED, ErrorCode::NORMAL, 0};
  last_joint_callback_time_ = std::chrono::steady_clock::now();
  last_temp_callback_time_ = std::chrono::steady_clock::now();
}

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

void GHandCallbackManager::UpdateJoints(const std::vector<Joint>& joints) {
  JointsCallback callback;
  std::vector<Joint> payload;
  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    // Always cache the lastest temp frame for GetJointsData() to poll
    last_joints_ = joints;
    // Change detection is based on the "last actually delivered value",
    // not on the previous polled frame.
    if (HasJointDataChanged(joints)) {
      callback = joints_callback_;
      last_delivered_joints_ = joints;  // Advance the baseline only when delivering
      last_joint_callback_time_ = std::chrono::steady_clock::now();
      payload = joints;  // Always deliver the full joint set, not a subset
    }
  }

  if (callback) {
    callback(payload);
  }
}

void GHandCallbackManager::UpdateTemperature(const HandState& temperature) {
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

void GHandCallbackManager::UpdateTactileData(const TactileData& data) {
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

HandState GHandCallbackManager::GetHandData() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  return last_state_;
}

std::vector<Joint> GHandCallbackManager::GetJointsData() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  return last_joints_;
}

TactileData GHandCallbackManager::GetTactileData() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  return last_tactile_data_;
}

bool GHandCallbackManager::HasJointDataChanged(
    const std::vector<Joint>& joints) {
  // First delivery
  if (last_delivered_joints_.empty()) {
    return true;
  }
  // 1. State/error changes -> deliver immediately
  for (const auto& joint : joints) {
    const Joint* previous = FindJointById(last_delivered_joints_, joint.id);
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
    const Joint* previous = FindJointById(last_delivered_joints_, joint.id);
    if (previous != nullptr &&
        std::abs(joint.angle - previous->angle) > kJointAngleThreshold) {
      return true;
    }
  }
  // 3. Freshness fallback: force delivery if no data has been delivered
  //    for more than 100 ms.
  auto now = std::chrono::steady_clock::now();
  auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - last_joint_callback_time_);
  if (age.count() > kDataFreshnessMs) {
    return true;
  }
  return false;
}

bool GHandCallbackManager::HasTemperatureChanged(
    const HandState& temperature) {
  // 1. State or error changes 鈫?trigger immediately
  if (temperature.state != last_state_.state ||
      temperature.error != last_state_.error) {
    GHAND_LOG_DEBUG(
        "Hand state change detected: " << ghand::ToString(temperature.state));
    return true;
  }

  // 2. Temperature change triggers (>1掳C)
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
