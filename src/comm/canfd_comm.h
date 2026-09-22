// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRC_COMM_CANFD_COMM_H_
#define SRC_COMM_CANFD_COMM_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "canfd_driver.h"
#include "icomm.h"
#include "product_config.h"

namespace ghand {
namespace internal {

/**
 * @brief CANFD communication implementation using Modbus-over-CANFD
 *
 * Implements the IComm interface, communicating with devices using Modbus
 * register access over CANFD frames (aligned with Python SDK behavior).
 */
class CANFDComm : public IComm {
 public:
  explicit CANFDComm(const ProductConfig& config);
  ~CANFDComm() override;

  CANFDComm(const CANFDComm&) = delete;
  CANFDComm& operator=(const CANFDComm&) = delete;
  CANFDComm(CANFDComm&&) = delete;
  CANFDComm& operator=(CANFDComm&&) = delete;

  // IComm interface implementation
  int Connect(const std::string& device_name, uint8_t slave_id,
              uint8_t baudrate_gear) override;
  int Disconnect() override;
  bool IsConnected() const override { return connected_.load(); }
  std::map<std::string, std::string> SearchAdapters() override;
  void UpdateConfig(const ProductConfig& config) override;
  bool SetSlaveId(uint16_t slave_index, uint8_t new_slave_id) override;
  bool SetBaudrateConfig(uint16_t slave_index, uint8_t baudrate_gear) override;
  Diagnostics GetDiagnostics() const override;

  DeviceInfo GetDeviceInfo(uint16_t slave_index) override;
  HandType GetHandType(uint16_t slave_index) override;
  std::vector<ConnectedHand> GetConnectedHands() override;

  bool MoveJoints(uint16_t slave_index,
                  const std::vector<JointCommand>& joints,
                  ControlMode mode) override;
  void Stop(uint16_t slave_index) override;

  bool ClearFault(uint16_t slave_index) override;

  bool OpenTactile(uint16_t slave_index) override;
  bool CloseTactile(uint16_t slave_index) override;
  bool ZeroTactile(uint16_t slave_index) override;

  void SetJointsCallback(JointsCallback cb) override;
  void SetHandStateCallback(HandStateCallback cb) override;
  void SetTactileDataCallback(TactileDataCallback cb) override;

  FirmwareUpdateError BootUpdate(
      const std::string& filename,
      std::function<void(int)> progress) override;
  bool QueryFirmwareUpdateResults(FirmwareUpdateResults* results) override;

  SelfTestErrorInfo GetSelfTestErrorInfo(uint16_t slave_index) override;
  uint8_t GetSelfTestStatus(uint16_t slave_index) override;

 private:
  // Resolve the slaves a call targets. slave_index 0 expands to every hand on
  // the bus; an unknown hand is rejected.
  bool TargetSlaves(uint16_t slave_index, std::vector<uint8_t>* slaves);
  // CANFD arbitration helpers (Python SDK compatible)
  static uint32_t PackArbitration(int src_id, int dst_id, int ack,
                                  int func_code, int start = 1, int end = 1,
                                  int toggle = 0, int seg_num = 0);
  static void UnpackArbitration(uint32_t can_id, int* src_id, int* dst_id,
                                int* ack, int* func_code, int* start, int* end,
                                int* toggle, int* seg_num);

  // Modbus-over-CANFD transport
  bool SendFrame(uint32_t can_id, const uint8_t* data, uint8_t len);
  bool RecvFrame(uint32_t* can_id, uint8_t* data, uint8_t* len,
                 int timeout_ms);

  bool ReadRegisters(uint8_t slave, int addr, int count,
                     std::vector<uint8_t>* out_bytes, int func_code = 0x04,
                     int timeout_ms = 500);
  bool WriteRegisters(uint8_t slave, int addr,
                      const std::vector<uint8_t>& data, int timeout_ms = 5000,
                      bool expect_response = true);

  bool WriteSingleRegister(uint8_t slave, int addr, uint16_t value,
                           int timeout_ms = 500);
  bool WaitHoldingResult(uint8_t slave, int addr, int timeout_ms = 2000,
                         int interval_ms = 50);

  bool NodeIdDetection(uint8_t slave, int timeout_ms = 500);
  bool EstablishConnection(uint8_t slave_id, int timeout_ms = 500);
  bool TryConnectSlave(uint8_t target,
                       const std::vector<std::vector<uint8_t>>& payloads,
                       int timeout_ms);
  void DeleteConnection();

  bool ReadInputBytes(uint8_t slave, int addr, int count,
                      std::vector<uint8_t>* bytes);
  std::vector<Joint> GetJoints(uint8_t slave, bool* ok = nullptr);
  HandState GetHandInfo(uint8_t slave, bool* ok = nullptr);
  TactileData GetTactileData(uint8_t slave, bool* ok = nullptr);
  DeviceInfo GetDeviceInfoForSlave(uint8_t slave);
  HandType GetHandTypeForSlave(uint8_t slave);

  bool WriteTactileControl(uint8_t slave, uint16_t command);
  void EnsurePollStarted();
  void StopPoll();
  void PollLoop();

  std::unique_ptr<CANFDDriver> driver_;
  std::mutex io_mutex_;
  uint8_t src_id_ = 0x0A;
  std::atomic<bool> connected_{false};

  // Hands found on the bus, in enumeration order. Guarded by hands_mutex_.
  std::vector<ConnectedHand> hands_;
  // Per-hand polling health, keyed by slave address. Guarded by hands_mutex_.
  struct HandHealth {
    int fail_count = 0;
    std::chrono::steady_clock::time_point last_tactile_time{};
    std::chrono::steady_clock::time_point skip_until{};
  };
  std::map<uint8_t, HandHealth> health_;
  mutable std::mutex hands_mutex_;

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
  bool joints_poll_enabled_ = false;
  bool hand_state_poll_enabled_ = false;
  bool tactile_poll_enabled_ = false;
  mutable std::mutex cb_mutex_;
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_COMM_CANFD_COMM_H_
