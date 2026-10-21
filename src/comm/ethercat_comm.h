// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRC_COMM_ETHERCAT_COMM_H_
#define SRC_COMM_ETHERCAT_COMM_H_

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "file_lock.h"
#include "icomm.h"
#include "product_config.h"

namespace ghand {
namespace internal {

constexpr size_t kFirmwareBufferSize = 8 * 1024 * 1024;

class EtherCATComm : public IComm {
 public:
  explicit EtherCATComm(const ProductConfig& config);
  ~EtherCATComm() override;

  EtherCATComm(const EtherCATComm&) = delete;
  EtherCATComm& operator=(const EtherCATComm&) = delete;
  EtherCATComm(EtherCATComm&&) = delete;
  EtherCATComm& operator=(EtherCATComm&&) = delete;

  // IComm interface implementation
  int Connect(const std::string& device_name, uint8_t slave_id,
              uint8_t baudrate_gear) override;
  int Disconnect() override;
  bool IsConnected() const override { return is_connected_; }
  std::map<std::string, std::string> SearchAdapters() override;
  void UpdateConfig(const ProductConfig& config) override;
  bool SetSlaveId(uint16_t slave, uint8_t slave_id) override;
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
  bool GetStallTolerances(uint16_t slave, std::map<JointId, uint8_t>* values) override;
  bool SetStallTolerances(uint16_t slave, const std::map<JointId, uint8_t>& values) override;
  bool GetPidParameters(uint16_t slave, std::vector<uint16_t>* values) override;
  bool SetPidParameters(uint16_t slave, const std::vector<uint16_t>& values) override;

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


  // Low-level EtherCAT API (retained for internal use)
  int SDORead(std::uint16_t slave, std::uint16_t index, std::uint8_t subindex,
              int* size, void* data, int timeout);

  int SDOWrite(std::uint16_t slave, std::uint16_t index, std::uint8_t subindex,
               int size, void* data, int timeout);

  int SendRxPDO(uint16_t slave_id, uint16_t pdo_index, uint32_t data_size,
                uint8_t* data);

  uint8_t* ReadTxPDO(uint16_t slave);

 private:
  friend struct ProtocolTestAccess;
  struct SoemState;

  // Instance state
  std::unique_ptr<SoemState> soem_;
  bool threads_started_ = false;

  int expectedWKC_ = 0;
  int wkc_ = 0;
  int mappingdone_ = 0;
  std::atomic<int> dorun_{0};
  int inOP_ = 0;
  bool is_connected_ = false;
  int dowkccheck_ = 0;
  int currentgroup_ = 0;
  int cycle_ = 0;
  int64_t cycletime_ = 10000000;
  std::atomic<uint64_t> tx_packets_{0};
  std::atomic<uint64_t> rx_packets_{0};
  std::atomic<uint64_t> tx_errors_{0};
  std::atomic<uint64_t> rx_errors_{0};
  std::atomic<uint64_t> reconnects_{0};
  std::atomic<uint64_t> received_frames_{0};
  std::atomic<uint64_t> dispatched_frames_{0};
  std::atomic<uint64_t> callback_errors_{0};

  uint8_t rxpdo_buffer_[2 + 12 * static_cast<size_t>(JointId::NUM_JOINTS)] = {0};
  size_t rxpdo_size_ = 0;

  std::mutex context_mutex_;
  std::mutex rt_context_mutex_;
  FileLock device_lock_;

  std::function<void(int)> state_update_callback_;

  char file_buffer_[kFirmwareBufferSize] = {0};

  // Callbacks
  JointsCallback joints_callback_;
  HandStateCallback hand_state_callback_;
  TactileDataCallback tactile_callback_;
  // Protects callback registration/copy only.
  // Never hold this mutex while invoking user callbacks.
  mutable std::mutex callback_mutex_;

  ProductConfig config_;
  std::map<uint16_t, std::map<JointId, JointCommand>> joint_commands_;
  std::map<uint16_t, ControlMode> hand_modes_;
  std::vector<ConnectedHand> hands_;
  std::vector<uint16_t> TargetSlaves(uint16_t slave);
  std::vector<std::vector<uint8_t>> rxpdo_buffers_;
  std::vector<size_t> rxpdo_sizes_;
  std::mutex command_mutex_;
  bool ExecuteSdoCommand(uint16_t slave, uint16_t index, uint8_t command,
                         uint8_t command_subindex = 1,
                         uint8_t state_subindex = 2, uint8_t result_subindex = 3);
  bool WriteSdoParameters(uint16_t slave, uint16_t index, const std::vector<uint8_t>& bytes);

  // FOE firmware update shared state
  static std::function<void(int)> progress_callback_;
  static EtherCATComm* foe_instance_;

  // Methods
  void ResetContext();
  bool InputBin(const char* fname, int* length);

  void StartThreads();
  void StopThreads();

  // Instance thread methods (called by static wrapper functions)
  void Ecatthread();
  void Ecatcheck();

  // Static thread entry points (required by OSAL to be C function pointers)
  static void EcatthreadWrapper(void* arg);
  static void EcatcheckWrapper(void* arg);

  // Firmware version helper
  std::string ReadFirmwareVersion(uint16_t slave, uint8_t mcu_id);
  FirmwareUpdateError CompleteFirmwareResetHandshake(uint16_t slave);

  // Static helper methods
  static void FoeProgressHook(uint16_t slave, int32_t packetnumber,
                              int32_t totalsize);

  // Parse PDO raw data and trigger structured callbacks
  void ParseHandAndJoints(const uint8_t* data, size_t size, size_t* offset,
                          std::vector<Joint>* parsed_joints,
                          HandState* parsed_temperature) const;
  TactileData ParseTactileData(const uint8_t* data, size_t size,
                               size_t* offset, bool compat_thumb = false) const;
  void ParseAndNotify(const uint8_t* data, size_t size, uint16_t slave);


};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_COMM_ETHERCAT_COMM_H_
