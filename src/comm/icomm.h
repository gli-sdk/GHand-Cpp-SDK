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

using JointsCallback = std::function<void(const std::vector<Joint>&)>;
using HandStateCallback = std::function<void(const HandState&)>;
using TactileDataCallback = std::function<void(const TactileData&)>;

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
  virtual bool SetSlaveId(uint8_t slave_id) = 0;
  virtual bool SetBaudrateConfig(uint8_t baudrate_gear) = 0;
  virtual bool PrepareBaudrateListener(const std::string& device_name,
                                       uint8_t baudrate_gear) {
    return false;
  }
  virtual Diagnostics GetDiagnostics() const { return Diagnostics(); }

  // Device info
  virtual DeviceInfo GetDeviceInfo() = 0;
  virtual HandType GetHandType() = 0;

  // Motion control
  virtual bool MoveJoints(const std::vector<JointCommand>& joints,
                          ControlMode mode) = 0;
  virtual void Stop() = 0;

  // System operations
  virtual bool ClearFault() = 0;
  virtual bool InitJoint() = 0;

  // Tactile sensor
  virtual bool OpenTactile() = 0;
  virtual bool CloseTactile() = 0;
  virtual bool ZeroTactile() = 0;

  // Data callbacks
  virtual void SetJointsCallback(JointsCallback cb) = 0;
  virtual void SetHandStateCallback(HandStateCallback cb) = 0;
  virtual void SetTactileDataCallback(TactileDataCallback cb) = 0;

  // Firmware update
  virtual FirmwareUpdateError BootUpdate(
      const std::string& filename,
      std::function<void(int)> progress) = 0;
  virtual bool QueryFirmwareUpdateResults(FirmwareUpdateResults* results) = 0;

  // Self-test error query (object dictionary 0x2008). Default implementation
  // returns an empty result for backends that do not support the query.
  virtual SelfTestErrorInfo GetSelfTestErrorInfo() { return SelfTestErrorInfo{}; }
  // Self-test status: 0 idle, 1 processing, 2 command processed successfully,
  // 3 failed.
  virtual uint8_t GetSelfTestStatus() { return 0; }
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_COMM_ICOMM_H_
