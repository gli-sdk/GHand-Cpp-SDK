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
  bool SetSlaveId(uint8_t slave_id) override;
  bool SetBaudrateConfig(uint8_t baudrate_gear) override;
  Diagnostics GetDiagnostics() const override;

  DeviceInfo GetDeviceInfo() override;
  HandType GetHandType() override;

  bool MoveJoints(const std::vector<JointCommand>& joints,
                  ControlMode mode) override;
  void Stop() override;

  bool ClearFault() override;
  bool InitJoint() override;

  bool OpenTactile() override;
  bool CloseTactile() override;
  bool ZeroTactile() override;

  void SetJointsCallback(JointsCallback cb) override;
  void SetHandStateCallback(HandStateCallback cb) override;
  void SetTactileDataCallback(TactileDataCallback cb) override;

  FirmwareUpdateError BootUpdate(
      const std::string& filename,
      std::function<void(int)> progress) override;
  bool QueryFirmwareUpdateResults(FirmwareUpdateResults* results) override;

  SelfTestErrorInfo GetSelfTestErrorInfo() override;
  uint8_t GetSelfTestStatus() override;

 private:
  bool ProbeSlave(int sid, int attempt, const std::string& device_name);
  void CloseContext();
  int ReadInputRegistersCounted(int addr, int count, uint16_t* dest);
  int ReadHoldingRegistersCounted(int addr, int count, uint16_t* dest);
  int WriteRegisterCounted(int addr, uint16_t value);
  int WriteRegistersCounted(int addr, int count, const uint16_t* data);
  bool WriteTactileControl(uint16_t command);
  bool WaitHoldingResult(int addr, int timeout_ms = 2000,
                         int interval_ms = 50);
  bool HasCallbacks();
  void EnsurePollStarted();
  void StopPoll();
  void PollLoop();

  bool ReadInputRegistersBytes(int addr, int count,
                               std::vector<uint8_t>* bytes);
  std::vector<Joint> GetJoints();
  HandState GetHandInfo();
  TactileData GetTactileData();

  modbus_t* ctx_ = nullptr;
  uint8_t slave_id_ = 0x31;
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

  // Callbacks
  JointsCallback joints_cb_;
  HandStateCallback hand_state_cb_;
  TactileDataCallback tactile_cb_;
  bool tactile_open_ = false;
  bool tactile_poll_enabled_ = false;
  mutable std::mutex cb_mutex_;
  std::mutex io_mutex_;
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_COMM_RS485_COMM_H_
