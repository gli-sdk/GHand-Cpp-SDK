// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRC_COMM_ICOMM_H_
#define SRC_COMM_ICOMM_H_

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "ghand/types.h"
#include "product_config.h"

namespace ghand {
namespace internal {

using JointsCallback =
    std::function<void(std::uint16_t slave, const std::vector<JointData>&)>;
using HandStateCallback =
    std::function<void(std::uint16_t slave, const HandData&, HandRuntimeState)>;
using TactileDataCallback =
    std::function<void(std::uint16_t slave, const std::map<TactileSensorId, TactileData>&)>;

// One hand found on the bus. Internal: the public API exposes hands as GHand
// objects instead.
struct ConnectedHand {
  // Transport-specific hand identifier: an EtherCAT slave index starting from
  // 1, or the RS485/CANFD slave address (e.g. 0x31).
  uint16_t slave_index = 0;
  HandType hand_type = HandType::NONE;
  DeviceInfo device_info;
};

struct FirmwareUpdateResults {
  uint8_t main = 0;
  uint8_t position_sensor = 0;
  uint8_t tactile_board = 0;
  uint8_t motor_driver = 0;
  uint8_t thumb_tactile = 0;
  uint8_t finger_tactile = 0;
};

/**
 * @brief Communication abstraction interface
 *
 * Provides a unified business-level communication interface for
 * EtherCAT/CANFD/RS485.
 * Implementations handle underlying protocol differences and provide
 * standardized device operations to upper layers.
 */
class IComm {
 public:
  virtual ~IComm() = default;

  // Connection management
  virtual int Connect(const std::string& device_name, uint8_t slave_id,
                      uint8_t baudrate_gear) = 0;
  virtual int Disconnect() = 0;
  virtual bool IsConnected() const = 0;
  virtual std::map<std::string, std::string> SearchAdapters() = 0;
  virtual void UpdateConfig(const ProductConfig& config) = 0;
  virtual bool SetSlaveId(uint16_t slave_index, uint8_t new_slave_id) = 0;
  virtual bool SetBaudrateConfig(uint16_t slave_index,
                                 uint8_t baudrate_gear) = 0;
  // Device info
  virtual DeviceInfo GetDeviceInfo(uint16_t slave_index) = 0;
  virtual HandType GetHandType(uint16_t slave_index) = 0;

  // Hand enumeration. slaver_index is transport-specific: an EtherCAT slave
  // index starting from 1, or the RS485/CANFD slave address (e.g. 0x31).
  virtual std::vector<ConnectedHand> GetConnectedHands() = 0;

  // slave_index == 0 acts on every enumerated hand using individually
  // addressed operations. Reads require one nonzero hand identifier.
  virtual bool MoveJoints(uint16_t slave_index,
                          const std::vector<JointCommand>& joints,
                          HandControlMode mode) = 0;
  virtual bool Stop(uint16_t slave_index) = 0;

  virtual bool DisableHand(uint16_t slave_index) { return false; }

  virtual bool GetHandStateMachine(uint16_t slave_index, HandRuntimeState* value) { return false; }

  virtual bool GetStallTolerances(uint16_t slave_index, std::map<JointId, uint8_t>* value) { return false; }

  virtual bool SetStallTolerances(uint16_t slave_index, const std::map<JointId, uint8_t>& values) { return false; }

  virtual bool GetPidParameters(uint16_t slave_index, std::vector<uint16_t>* value) { return false; }

  virtual bool SetPidParameters(uint16_t slave_index, const std::vector<uint16_t>& values) { return false; }

  // System operations
  virtual bool ClearFault(uint16_t slave_index) = 0;

  // Tactile sensor
  virtual bool OpenTactile(uint16_t slave_index) = 0;
  virtual bool CloseTactile(uint16_t slave_index) = 0;
  virtual bool ZeroTactile(uint16_t slave_index) = 0;

  // Data callbacks
  virtual void SetJointsCallback(JointsCallback cb) = 0;
  virtual void SetHandStateCallback(HandStateCallback cb) = 0;
  virtual void SetTactileDataCallback(TactileDataCallback cb) = 0;

  // Firmware update
  virtual FirmwareUpdateError BootUpdate(
      const std::string& filename,
      std::function<void(int)> progress) = 0;
  virtual bool QueryFirmwareUpdateResults(FirmwareUpdateResults* results) = 0;


};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_COMM_ICOMM_H_
