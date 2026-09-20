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

#include "ghand/types.h"

namespace ghand {
namespace internal {

/**
 * @brief GHand callback manager internal implementation class
 *
 * Responsibilities:
 * 1. Manage registration of various data callbacks
 * 2. Detect data changes
 * 3. Trigger corresponding callbacks when data changes
 * 4. Ensure data freshness (forced update every 100 ms)
 * 5. Cache the latest data per EtherCAT slave to support polled access
 *
 * Multi-slave EtherCAT buses deliver one feedback frame per slave (e.g. a
 * junction/splitter at slave 1 with the hands at slaves 2..N). Each slave's
 * frame is cached under its own slave index; user callbacks are only fired
 * for the currently selected slave so single-hand semantics stay intact.
 */
class GHandCallbackManager {
 public:
  // Callback type definitions
  using JointsCallback = std::function<void(const std::vector<Joint>&)>;
  using HandStateCallback = std::function<void(const HandState&)>;
  using TactileDataCallback = std::function<void(const TactileData&)>;

  GHandCallbackManager();
  ~GHandCallbackManager() = default;

  GHandCallbackManager(const GHandCallbackManager&) = delete;
  GHandCallbackManager& operator=(const GHandCallbackManager&) = delete;
  GHandCallbackManager(GHandCallbackManager&&) = delete;
  GHandCallbackManager& operator=(GHandCallbackManager&&) = delete;

  // Callback registration methods
  void SetJointsCallback(JointsCallback callback);
  void SetHandStateCallback(HandStateCallback callback);
  void SetTactileDataCallback(TactileDataCallback callback);

  // Slave selection: user callbacks fire only for the selected slave.
  void SetSelectedSlave(uint16_t slave);

  // Data update methods (called by GHand)
  void UpdateJoints(uint16_t slave, const std::vector<Joint>& joints);
  void UpdateTemperature(uint16_t slave, const HandState& temperature);
  void UpdateTactileData(uint16_t slave, const TactileData& data);

  // Polled access methods (thread-safe)
  HandState GetHandData(uint16_t slave) const;
  std::vector<Joint> GetJointsData(uint16_t slave) const;
  TactileData GetTactileData(uint16_t slave) const;

 private:
  // Per-slave cache: latest data + change detection state.
  struct SlaveCache {
    std::vector<Joint> joints;
    std::vector<Joint> delivered_joints;  // as change detection baseline
    HandState state{State::STOPPED, ErrorCode::NORMAL, 0};
    TactileData tactile;
    bool has_tactile = false;
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

  // Callback member variables
  JointsCallback joints_callback_;
  HandStateCallback hand_state_callback_;
  TactileDataCallback tactile_data_callback_;

  // Last data caches (used for change detection + polled access)
  mutable std::mutex data_mutex_;
  std::map<uint16_t, SlaveCache> caches_;
  uint16_t selected_slave_ = 1;

  // Change detection threshold constants
  static constexpr float kJointAngleThreshold = 1.0f;   // 1 degree
  static constexpr float kTemperatureThreshold = 1.0f;  // 1 degree Celsius
  // 100 ms data freshness.
  static constexpr int kDataFreshnessMs = 100;
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_GHAND_CALLBACK_MANAGER_H_
