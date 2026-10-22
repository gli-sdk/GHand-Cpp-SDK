// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRC_INTERNAL_GHAND_IMPL_H_
#define SRC_INTERNAL_GHAND_IMPL_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ghand/result.h"
#include "ghand/types.h"
#include "icomm.h"
#include "product_config.h"

namespace ghand {
namespace internal {

class GHandCallbackManager;

/**
 * @brief GHand internal implementation class
 *
 * Owns one communication context (one adapter or serial port) and the hands
 * found on that bus. The public Master/GHand objects forward here; every
 * fallible call reports its own outcome through Result / ResultOf, so no state
 * has to be remembered between calls.
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
  ResultOf<std::vector<ConnectedHand>> Connect(const std::string& device_name,
                                               uint8_t slave_id = 0);
  ResultOf<std::vector<ConnectedHand>> Connect(const std::string& device_name,
                                               uint8_t slave_id,
                                               uint8_t baudrate_gear);
  ResultOf<std::vector<ConnectedHand>> Connect(const std::string& device_name,
                                               uint8_t slave_id,
                                               RS485BaudRate baud_rate);
  ResultOf<std::vector<ConnectedHand>> Connect(const std::string& device_name,
                                               uint8_t slave_id,
                                               CANFDBitTiming timing);
  Result Disconnect();
  bool IsConnected() const;
  Result SetSlaveId(uint16_t slave_index, uint8_t new_slave_id);
  Result SetBaudrateConfig(uint16_t slave_index, RS485BaudRate baud_rate);
  Result SetBaudrateConfig(uint16_t slave_index, CANFDBitTiming timing);
  Result PrepareCANFDBaudrateListener(const std::string& device_name,
                                      CANFDBitTiming timing);
  DeviceInfo GetLastDeviceInfo() const { return device_info_; }
  Diagnostics GetDiagnostics() const;

  // Device info
  std::map<std::string, std::string> SearchAdapters() const;
  ResultOf<HandType> GetHandType(uint16_t slave_index) const;
  ResultOf<DeviceInfo> GetDeviceInfo(uint16_t slave_index) const;

  // Control operations
  Result MoveJoints(uint16_t slave_index,
                    const std::vector<JointCommand>& joints, ControlMode mode);
  Result Stop(uint16_t slave_index);
  Result DisableHand(uint16_t slave_index);
  ResultOf<HandStateMachine> GetHandStateMachine(uint16_t slave_index);
  ResultOf<std::map<JointId, uint8_t>> GetStallTolerances(uint16_t slave_index);
  Result SetStallTolerances(uint16_t slave_index, const std::map<JointId, uint8_t>& values);
  ResultOf<std::vector<uint16_t>> GetPidParameters(uint16_t slave_index);
  Result SetPidParameters(uint16_t slave_index, const std::vector<uint16_t>& values);
  Result ClearFault(uint16_t slave_index);

  // Tactile sensor control
  Result OpenTactile(uint16_t slave_index);
  Result CloseTactile(uint16_t slave_index);
  Result ZeroTactile(uint16_t slave_index);

  // Callback registration (one slot per hand)
  void SetJointsCallback(
      uint16_t slave_index,
      std::function<void(const std::vector<Joint>& joints)> cb);
  void SetHandStateCallback(
      uint16_t slave_index,
      std::function<void(const HandState& state)> cb);
  void SetTactileDataCallback(
      uint16_t slave_index,
      std::function<void(const TactileData& data)> cb);

  // Data polling
  ResultOf<HandState> GetHandData(uint16_t slave_index) const;
  ResultOf<std::vector<Joint>> GetJointsData(uint16_t slave_index) const;
  ResultOf<TactileData> GetTactileData(uint16_t slave_index) const;


  // Firmware update
  FirmwareUpdateError BootUpdate(
      const std::string& filename,
      std::function<void(int)> progress_callback);

 private:
  ResultOf<std::vector<ConnectedHand>> AutoConnect(uint8_t slave_id,
                                                   uint8_t baudrate_gear);
  ResultOf<std::vector<ConnectedHand>> ConnectToDevice(
      const std::string& device_name, uint8_t slave_id, uint8_t baudrate_gear);
  void SetupCallbacks();
  void ClampJointAngle(JointCommand& joint);

  std::unique_ptr<IComm> comm_;
  std::unique_ptr<GHandCallbackManager> callback_manager_;

  CommType comm_type_;
  ProductType product_type_;
  std::string device_name_;
  DeviceInfo device_info_;
  ProductConfig config_;
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_GHAND_IMPL_H_
