#ifndef GHAND_H_
#define GHAND_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ghand/export.h"
#include "ghand/types.h"
#include "ghand/version.h"

namespace ghand {

// Forward declaration of internal implementation class
namespace internal {
class DexHand;
}  // namespace internal

// Callback type definitions
using JointsCallback = std::function<void(const std::vector<Joint>&)>;
using HandStateCallback = std::function<void(const HandState&)>;
using TactileDataCallback = std::function<void(const TactileData&)>;

/**
 * @brief GHand DexHand robotic hand control interface
 *
 * This is the sole public API class, providing full control of the robotic
 * hand.
 */
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
class GHAND_API DexHand {
 public:
  static std::unique_ptr<DexHand> Create(
      ProductType product_type = ProductType::AUTO,
      CommType comm_type = CommType::ETHERCAT);
  ~DexHand();

  // DexHand owns device state and is not copyable or movable.
  DexHand(const DexHand&) = delete;
  DexHand& operator=(const DexHand&) = delete;
  DexHand(DexHand&&) = delete;
  DexHand& operator=(DexHand&&) = delete;

  // Connection management
  /**
   * @brief Auto-connect to device
   * @return true on success
   */
  bool AutoConnect();

  /**
   * @brief Connect to specified device
   * @param device_name Device name ("auto" for auto-discovery)
   * @return true on success
   */
  bool Connect(const std::string& device_name = "auto");

  /**
   * @brief Disconnect
   */
  bool Disconnect();

  /**
   * @brief Check if connected
   */
  bool IsConnected() const;

  // Device info
  /**
   * @brief Search available communication adapters
   * @return Adapter name mapping table
   */
  std::map<std::string, std::string> SearchAdapters();

  /**
   * @brief Get hand type
   */
  HandType GetHandType();

  /**
   * @brief Get device info
   */
  DeviceInfo GetDeviceInfo();

  // Motion control
  /**
   * @brief Set control mode
   */
  void SetControlMode(ControlMode mode);

  /**
   * @brief Control joint movement
   * @param joints Joint command list, angle in degrees (deg)
   * @return true on success
   */
  bool MoveJoints(const std::vector<JointCommand>& joints);

  /**
   * @brief Stop all motion immediately
   */
  void Stop();

  /**
   * @brief Clear fault state
   */
  bool ClearFault();

  /**
   * @brief Initialize joints
   */
  bool InitJoint();

  // Tactile sensor
  /**
   * @brief Open tactile sensor
   */
  bool OpenTactile();

  /**
   * @brief Close tactile sensor
   */
  bool CloseTactile();

  /**
   * @brief Zero tactile sensor
   */
  bool ZeroTactile();

  // Data access
  /**
   * @brief Get hand state (latest cached data, no hardware read triggered)
   */
  HandState GetHandData();

  /**
   * @brief Get joint data (latest cached data, no hardware read triggered)
   */
  std::vector<Joint> GetJointsData();

  /**
   * @brief Get tactile data (latest cached data, no hardware read triggered)
   */
  TactileData GetTactileData();

  // Callback registration
  /**
   * @brief Register joint data callback
   * @param cb Callback function
   */
  void SetJointsCallback(JointsCallback cb);

  /**
   * @brief Register hand state callback
   * @param cb Callback function
   */
  void SetHandStateCallback(HandStateCallback cb);

  /**
   * @brief Register tactile data callback
   * @param cb Callback function
   */
  void SetTactileDataCallback(TactileDataCallback cb);

  // Firmware update
  FirmwareUpdateError BootUpdate(
      const std::string& filename,
      std::function<void(int)> progress_callback);

 private:
  DexHand(ProductType product_type, CommType comm_type);
  std::unique_ptr<internal::DexHand> impl_;
};

}  // namespace ghand

#ifdef _WIN32
#pragma warning(pop)
#endif

#endif  // GHAND_H_
