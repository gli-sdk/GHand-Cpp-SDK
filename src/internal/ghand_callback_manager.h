// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRC_INTERNAL_GHAND_CALLBACK_MANAGER_H_
#define SRC_INTERNAL_GHAND_CALLBACK_MANAGER_H_

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

#include "ghand/result.h"
#include "ghand/types.h"

namespace ghand {
namespace internal {

/**
 * @brief GHand callback manager internal implementation class
 *
 * Responsibilities:
 * 1. Hold one callback slot per hand (the hand is named at registration)
 * 2. Detect data changes
 * 3. Fire the callback registered for the hand the data belongs to
 * 4. Ensure data freshness (forced update every 100 ms)
 * 5. Cache the latest data per slave to support polled access
 */
class GHandCallbackManager {
 public:
  // Callback type definitions. The hand is named at registration time, so the
  // payload carries no slave index.
  using JointsCallback = std::function<void(const std::vector<Joint>& joints)>;
  using HandStateCallback = std::function<void(const HandState& state)>;
  using TactileDataCallback = std::function<void(const TactileData& data)>;

  GHandCallbackManager() = default;
  ~GHandCallbackManager() = default;

  GHandCallbackManager(const GHandCallbackManager&) = delete;
  GHandCallbackManager& operator=(const GHandCallbackManager&) = delete;
  GHandCallbackManager(GHandCallbackManager&&) = delete;
  GHandCallbackManager& operator=(GHandCallbackManager&&) = delete;

  void Clear();
  void RenameHand(uint16_t from, uint16_t to);

  // Callback registration methods: one slot per hand.
  void SetJointsCallback(uint16_t slave, JointsCallback callback);
  void SetHandStateCallback(uint16_t slave, HandStateCallback callback);
  void SetTactileDataCallback(uint16_t slave, TactileDataCallback callback);

  // Data update methods (called by GHand)
  void UpdateJoints(uint16_t slave, const std::vector<Joint>& joints);
  void UpdateTemperature(uint16_t slave, const HandState& temperature);
  void UpdateTactileData(uint16_t slave, const TactileData& data);

  // Polled access methods (thread-safe)
  ResultOf<HandState> GetHandData(uint16_t slave) const;
  ResultOf<std::vector<Joint>> GetJointsData(uint16_t slave) const;
  ResultOf<TactileData> GetTactileData(uint16_t slave) const;

 private:
  // Per-slave cache: latest data + change detection state.
  struct SlaveCache {
    std::vector<Joint> joints;
    std::vector<Joint> delivered_joints;  // as change detection baseline
    HandState state{};
    TactileData tactile;
    std::chrono::steady_clock::time_point last_joint_callback_time{
        std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point last_temp_callback_time{
        std::chrono::steady_clock::now()};
  };

  // Change detection methods
  bool HasJointDataChanged(const SlaveCache& cache,
                           const std::vector<Joint>& joints);
  bool HasTemperatureChanged(const SlaveCache& cache,
                             const HandState& temperature);

  // Callback slots, one per registered hand
  std::map<uint16_t, JointsCallback> joints_callbacks_;
  std::map<uint16_t, HandStateCallback> hand_state_callbacks_;
  std::map<uint16_t, TactileDataCallback> tactile_data_callbacks_;

  // Last data caches (used for change detection + polled access)
  mutable std::mutex data_mutex_;
  std::map<uint16_t, SlaveCache> caches_;

  // Change detection threshold constants
  static constexpr float kJointAngleThreshold = 1.0f;   // 1 degree
  static constexpr float kTemperatureThreshold = 1.0f;  // 1 degree Celsius
  // 100 ms data freshness.
  static constexpr int kDataFreshnessMs = 100;
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_GHAND_CALLBACK_MANAGER_H_
