#ifndef SRC_INTERNAL_ICOMM_H_
#define SRC_INTERNAL_ICOMM_H_

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "ghand/types.h"

namespace ghand {
namespace internal {

using JointsCallback = std::function<void(const std::vector<Joint>&)>;
using HandStateCallback = std::function<void(const HandState&)>;
using TactileDataCallback = std::function<void(const TactileData&)>;

/**
 * @brief Communication abstraction interface
 *
 * Provides a unified business-level communication interface for EtherCAT/CANFD/RS485.
 * Implementations handle underlying protocol differences and provide standardized device operations to upper layers.
 */
class IComm {
 public:
  virtual ~IComm() = default;

  // ===== Connection Management =====
  virtual int Connect(const std::string& device_name) = 0;
  virtual int Disconnect() = 0;
  virtual bool IsConnected() const = 0;
  virtual std::map<std::string, std::string> SearchAdapters() = 0;

  // ===== Device Info =====
  virtual DeviceInfo GetDeviceInfo() = 0;
  virtual HandType GetHandType() = 0;

  // ===== Motion Control =====
  virtual bool MoveJoints(const std::vector<JointCommand>& joints,
                          ControlMode mode) = 0;
  virtual void Stop() = 0;

  // ===== System Operations =====
  virtual bool ClearFault() = 0;
  virtual bool InitJoint() = 0;

  // ===== Tactile Sensor =====
  virtual bool OpenTactile() = 0;
  virtual bool CloseTactile() = 0;
  virtual bool ZeroTactile() = 0;

  // ===== Data Callbacks =====
  virtual void SetJointsCallback(JointsCallback cb) = 0;
  virtual void SetHandStateCallback(HandStateCallback cb) = 0;
  virtual void SetTactileDataCallback(TactileDataCallback cb) = 0;

  // ===== Firmware Update =====
  virtual FirmwareUpdateError BootUpdate(
      const std::string& filename,
      std::function<void(int)> progress) = 0;
  virtual bool QueryFirmwareUpdateResults(uint8_t* main_result,
                                          uint8_t* pos_result,
                                          uint8_t* tac_result,
                                          uint8_t* motor_result) = 0;
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_ICOMM_H_
