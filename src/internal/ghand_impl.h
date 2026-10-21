// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRC_INTERNAL_GHAND_IMPL_H_
#define SRC_INTERNAL_GHAND_IMPL_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ghand/types.h"
#include "product_config.h"

namespace ghand {
namespace internal {

class IComm;
class GHandCallbackManager;

/**
 * @brief GHand internal implementation class
 *
 * This class contains all implementation details of GHand, separated from
 * the public API via the Pimpl idiom.
 * User code should not access this class directly; it is intended for SDK
 * internal use only.
 */
class GHandImpl {
 public:
  explicit GHandImpl(ProductType product_type, CommType comm_type);
  ~GHandImpl();

  GHandImpl(const GHandImpl&) = delete;
  GHandImpl& operator=(const GHandImpl&) = delete;
  GHandImpl(GHandImpl&&) = delete;
  GHandImpl& operator=(GHandImpl&&) = delete;

  bool IsValid() const { return comm_ != nullptr; }

  // Connection management
  bool AutoConnect();
  bool AutoConnect(uint8_t slave_id, uint8_t baudrate_gear);
  bool Connect(const std::string& device_name, uint8_t slave_id = 0,
               uint8_t baudrate_gear = 0x05);
  bool Connect(const std::string& device_name, uint8_t slave_id,
               RS485BaudRate baud_rate);
  bool Connect(const std::string& device_name, uint8_t slave_id,
               CANFDBitTiming timing);
  bool Disconnect();
  bool IsConnected() const;
  bool SetSlaveId(uint8_t slave_id);
  bool SetBaudrateConfig(RS485BaudRate baud_rate);
  bool SetBaudrateConfig(CANFDBitTiming timing);
  OperationResult GetLastResult() const;
  Diagnostics GetDiagnostics() const;

  // Device info
  std::map<std::string, std::string> SearchAdapters() const;
  HandType GetHandType() const;
  DeviceInfo GetDeviceInfo() const;

  // Control operations
  void SetControlMode(ControlMode mode);
  bool MoveJoints(const std::vector<JointCommand>& joints);
  void Stop();

  // New protocol controls; queries write their result only on success.
  bool DisableHand();
  bool GetHandStateMachine(HandStateMachine* state);
  bool GetStallTolerances(std::map<JointId, uint8_t>* values);
  bool SetStallTolerances(const std::map<JointId, uint8_t>& values);
  bool GetPidParameters(std::vector<uint16_t>* values);
  bool SetPidParameters(const std::vector<uint16_t>& values);
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

  // Legacy self-test queries: report NOT_SUPPORTED without device traffic.
  SelfTestErrorInfo GetSelfTestErrorInfo();
  uint8_t GetSelfTestStatus();

  // Firmware update
  FirmwareUpdateError BootUpdate(
      const std::string& filename,
      std::function<void(int)> progress_callback);

 private:
  bool ConnectToDevice(const std::string& device_name, uint8_t slave_id,
                       uint8_t baudrate_gear);
  void SetLastResult(SdkError code, const std::string& message);
  void SetupCallbacks();
  void ClampJointAngle(JointCommand& joint);

  std::unique_ptr<IComm> comm_;
  std::unique_ptr<GHandCallbackManager> callback_manager_;

  ControlMode control_mode_;
  CommType comm_type_;
  ProductType product_type_;
  std::string device_name_;
  ProductConfig config_;
  mutable std::mutex last_result_mutex_;
  OperationResult last_result_;
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_GHAND_IMPL_H_
