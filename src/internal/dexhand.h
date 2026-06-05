#ifndef SRC_INTERNAL_DEXHAND_H_
#define SRC_INTERNAL_DEXHAND_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ghand/types.h"
#include "product_config.h"

namespace ghand {
namespace internal {

class IComm;
class DexHandCallbackManager;

/**
 * @brief DexHand internal implementation class
 *
 * This class contains all implementation details of DexHand, separated from the public API via the Pimpl idiom.
 * User code should not access this class directly; it is intended for SDK internal use only.
 */
class DexHand {
 public:
  explicit DexHand(ProductType product_type, CommType comm_type);
  ~DexHand();

  bool IsValid() const { return comm_ != nullptr; }

  // Connection management
  bool AutoConnect();
  bool Connect(const std::string& device_name);
  bool Disconnect();
  bool IsConnected() const;

  // Device info
  std::map<std::string, std::string> SearchAdapters() const;
  HandType GetHandType() const;
  DeviceInfo GetDeviceInfo() const;

  // Control operations
  void SetControlMode(ControlMode mode);
  bool MoveJoints(const std::vector<JointCommand>& joints);
  void Stop();
  bool ClearFault();
  bool InitJoint();

  // Tactile sensor control
  bool OpenTactile();
  bool CloseTactile();
  bool ZeroTactile();

  // Callback registration
  void SetJointsCallback(std::function<void(const std::vector<Joint>&)> cb);
  void SetHandStateCallback(std::function<void(const HandState&)> cb);
  void SetTactileDataCallback(std::function<void(const TactileData&)> cb);

  // Data polling
  HandState GetHandData() const;
  std::vector<Joint> GetJointsData() const;
  TactileData GetTactileData() const;

  // Firmware update
  FirmwareUpdateError BootUpdate(
      const std::string& filename,
      std::function<void(int)> progress_callback);

 private:
  bool ConnectToDevice(const std::string& device_name);
  void SetupCallbacks();
  void ClampJointAngle(JointCommand& joint);
  void ClampJointVelocity(JointCommand& joint);
  void ClampJointTorque(JointCommand& joint);

  std::unique_ptr<IComm> comm_;
  std::unique_ptr<DexHandCallbackManager> callback_manager_;

  ControlMode control_mode_;
  CommType comm_type_;
  ProductType product_type_;
  std::string device_name_;
  ProductConfig config_;
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_DEXHAND_H_
