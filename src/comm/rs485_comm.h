// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRC_COMM_RS485_COMM_H_
#define SRC_COMM_RS485_COMM_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "icomm.h"
#include "product_config.h"

#ifdef GHAND_NO_LIBMODBUS
// Stub modbus context when libmodbus is not available
struct modbus_t;
#else
struct _modbus;
using modbus_t = _modbus;
#endif

namespace ghand {
namespace internal {

/**
 * @brief RS485 communication implementation using Modbus RTU
 *
 * Implements the IComm interface, communicating with devices via serial port
 * using the libmodbus library.
 */
class RS485Comm : public IComm {
 public:
  explicit RS485Comm(const ProductConfig& config);
  ~RS485Comm() override;

  RS485Comm(const RS485Comm&) = delete;
  RS485Comm& operator=(const RS485Comm&) = delete;
  RS485Comm(RS485Comm&&) = delete;
  RS485Comm& operator=(RS485Comm&&) = delete;

  // IComm interface implementation
  int Connect(const std::string& device_name, uint8_t slave_id,
              uint8_t baudrate_gear) override;
  int Disconnect() override;
  bool IsConnected() const override { return connected_.load(); }
  std::map<std::string, std::string> SearchAdapters() override;
  void UpdateConfig(const ProductConfig& config) override;
  bool SetSlaveId(uint16_t slave, uint8_t new_slave_id) override;
  bool SetBaudrateConfig(uint16_t slave, uint8_t baudrate_gear) override;
  Diagnostics GetDiagnostics() const override;
  std::vector<ConnectedHand> GetConnectedHands() override;

  DeviceInfo GetDeviceInfo(uint16_t slave) override;
  HandType GetHandType(uint16_t slave) override;

  bool MoveJoints(uint16_t slave, const std::vector<JointCommand>& joints,
                  ControlMode mode) override;
  bool Stop(uint16_t slave) override;
  bool DisableHand(uint16_t slave) override;
  bool GetHandStateMachine(uint16_t slave, HandStateMachine* state) override;

  bool ClearFault(uint16_t slave) override;

  bool OpenTactile(uint16_t slave) override;
  bool CloseTactile(uint16_t slave) override;
  bool ZeroTactile(uint16_t slave) override;

  void SetJointsCallback(JointsCallback cb) override;
  void SetHandStateCallback(HandStateCallback cb) override;
  void SetTactileDataCallback(TactileDataCallback cb) override;

  FirmwareUpdateError BootUpdate(
      const std::string& filename,
      std::function<void(int)> progress) override;
  bool QueryFirmwareUpdateResults(FirmwareUpdateResults* results) override;


 private:
  friend struct ProtocolTestAccess;
  bool ProbeSlave(int sid, int attempt, const std::string& device_name);
  void CloseContext();
  int ReadInputRegistersCounted(uint16_t slave, int addr, int count, uint16_t* dest);
  int ReadHoldingRegistersCounted(uint16_t slave, int addr, int count, uint16_t* dest);
  int WriteRegisterCounted(uint16_t slave, int addr, uint16_t value);
  int WriteRegistersCounted(uint16_t slave, int addr, int count, const uint16_t* data);
  bool WriteTactileControl(uint16_t slave, uint16_t command);
  bool WaitInputResult(uint16_t slave, int addr, int baseline = -1);
  bool HasCallbacks();
  void EnsurePollStarted();
  void StopPoll();
  void PollLoop();

  bool ReadInputRegistersBytes(uint16_t slave, int addr, int count,
                               std::vector<uint8_t>* bytes);
  std::vector<Joint> GetJoints(uint16_t slave);
  HandState GetHandInfo(uint16_t slave, bool* ok = nullptr);
  TactileData GetTactileData(uint16_t slave);

  modbus_t* ctx_ = nullptr;
  std::atomic<bool> connected_{false};

  std::thread poll_thread_;
  std::atomic<bool> poll_running_{false};
  std::atomic<bool> poll_stop_{false};
  std::atomic<uint64_t> tx_packets_{0};
  std::atomic<uint64_t> rx_packets_{0};
  std::atomic<uint64_t> tx_errors_{0};
  std::atomic<uint64_t> rx_errors_{0};
  std::atomic<uint64_t> timeouts_{0};
  std::atomic<uint64_t> callback_errors_{0};

  ProductConfig config_;
  std::map<uint16_t, ControlMode> hand_modes_;
  std::map<uint16_t, std::map<JointId, ControlMode>> joint_modes_;
  std::map<uint16_t, ConnectedHand> hands_;
  std::map<uint16_t, bool> tactile_open_hands_;
  mutable std::mutex hands_mutex_;
  std::vector<uint16_t> TargetSlaves(uint16_t slave);
  std::mutex command_mutex_;

  // Callbacks
  JointsCallback joints_cb_;
  HandStateCallback hand_state_cb_;
  TactileDataCallback tactile_cb_;
  mutable std::mutex cb_mutex_;
  std::mutex io_mutex_;
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_COMM_RS485_COMM_H_
