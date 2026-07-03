#ifndef GHAND_INTERNAL_DEXHAND_CALLBACK_MANAGER_H_
#define GHAND_INTERNAL_DEXHAND_CALLBACK_MANAGER_H_

#include <chrono>
#include <functional>
#include <mutex>
#include <vector>

#include "ghand/types.h"

namespace ghand {
namespace internal {

/**
 * @brief DexHand callback manager internal implementation class
 *
 * Responsibilities:
 * 1. Manage registration of various data callbacks
 * 2. Detect data changes
 * 3. Trigger corresponding callbacks when data changes
 * 4. Ensure data freshness (forced update every 100 ms)
 * 5. Cache the latest data to support polled access
 */
class DexHandCallbackManager {
 public:
  // Callback type definitions
  using JointsCallback = std::function<void(const std::vector<Joint>&)>;
  using HandStateCallback = std::function<void(const HandState&)>;
  using TactileDataCallback = std::function<void(const TactileData&)>;

  DexHandCallbackManager();
  ~DexHandCallbackManager() = default;

  DexHandCallbackManager(const DexHandCallbackManager&) = delete;
  DexHandCallbackManager& operator=(const DexHandCallbackManager&) = delete;
  DexHandCallbackManager(DexHandCallbackManager&&) = delete;
  DexHandCallbackManager& operator=(DexHandCallbackManager&&) = delete;

  // Callback registration methods
  void SetJointsCallback(JointsCallback callback);
  void SetHandStateCallback(HandStateCallback callback);
  void SetTactileDataCallback(TactileDataCallback callback);

  // Data update methods (called by DexHand)
  void UpdateJoints(const std::vector<Joint>& joints);
  void UpdateTemperature(const HandState& temperature);
  void UpdateTactileData(const TactileData& data);

  // Polled access methods (thread-safe)
  HandState GetHandData() const;
  std::vector<Joint> GetJointsData() const;
  TactileData GetTactileData() const;

 private:
  // Change detection methods
  bool HasJointDataChanged(const std::vector<Joint>& joints);
  bool HasTemperatureChanged(const HandState& temperature);

  // Callback member variables
  JointsCallback joints_callback_;
  HandStateCallback hand_state_callback_;
  TactileDataCallback tactile_data_callback_;

  // Last data cache (used for change detection + polled access)
  mutable std::mutex data_mutex_;
  std::vector<Joint> last_joints_;
  std::vector<Joint> last_delivered_joints_;  // as change detection baseline
  HandState last_state_;
  TactileData last_tactile_data_;
  bool has_tactile_data_ = false;
  std::chrono::steady_clock::time_point last_joint_callback_time_;
  std::chrono::steady_clock::time_point last_temp_callback_time_;

  // Change detection threshold constants
  static constexpr float kJointAngleThreshold = 1.0f;   // 1 degree
  static constexpr float kTemperatureThreshold = 1.0f;  // 1 degree Celsius
  // 100 ms data freshness.
  static constexpr int kDataFreshnessMs = 100;
};

}  // namespace internal
}  // namespace ghand

#endif  // GHAND_INTERNAL_DEXHAND_CALLBACK_MANAGER_H_
