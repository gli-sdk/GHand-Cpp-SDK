// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRC_COMM_CANFD_COMM_H_
#define SRC_COMM_CANFD_COMM_H_

#include <atomic>
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
  bool PrepareBaudrateListener(const std::string& device_name, uint8_t gear) override;
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

  bool ReadRegisters(uint16_t slave, int addr, int count, std::vector<uint8_t>* out_bytes,
                     int func_code = 0x04, int timeout_ms = 100);
  bool WriteRegisters(uint16_t slave, int addr, const std::vector<uint8_t>& data,
                      int timeout_ms = 100);

  bool WriteSingleRegister(uint16_t slave, int addr, uint16_t value, int timeout_ms = 100);
  bool WaitInputResult(uint16_t slave, int addr, int baseline = -1);

  bool DetectNode(uint8_t slave_id, int timeout_ms = 500);
  bool EstablishConnection(uint8_t slave_id, int timeout_ms = 100);
  void DeleteConnection(uint16_t slave);

  bool ReadInputBytes(uint16_t slave, int addr, int count, std::vector<uint8_t>* bytes);
  std::vector<Joint> GetJoints(uint16_t slave);
  HandState GetHandInfo(uint16_t slave, bool* ok = nullptr);
  TactileData GetTactileData(uint16_t slave);

  bool WriteTactileControl(uint16_t slave, uint16_t command);
  void EnsurePollStarted();
  void StopPoll();
  void PollLoop();

  std::unique_ptr<CANFDDriver> driver_;
  std::mutex io_mutex_;
  uint8_t src_id_ = 0x0A;
  std::atomic<bool> connected_{false};

  std::thread keepalive_thread_;
  std::atomic<bool> keepalive_stop_{true};
  void KeepaliveLoop();
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
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_COMM_CANFD_COMM_H_
