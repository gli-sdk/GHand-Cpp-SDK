#ifndef GHAND_INTERNAL_ETHERCAT_COMM_H_
#define GHAND_INTERNAL_ETHERCAT_COMM_H_

#include <atomic>
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
  int Connect(const std::string& device_name) override;
  int Disconnect() override;
  bool IsConnected() const override { return is_connected_; }
  std::map<std::string, std::string> SearchAdapters() override;

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
  bool QueryFirmwareUpdateResults(uint8_t* main_result, uint8_t* pos_result,
                                  uint8_t* tac_result,
                                  uint8_t* motor_result) override;

  // Low-level EtherCAT API (retained for internal use)
  int SDORead(std::uint16_t slave, std::uint16_t index, std::uint8_t subindex,
              int* size, void* data, int timeout);

  int SDOWrite(std::uint16_t slave, std::uint16_t index, std::uint8_t subindex,
               int size, void* data, int timeout);

  int SendRxPDO(uint16_t slave_id, uint16_t pdo_index, uint32_t data_size,
                uint8_t* data);

  uint8_t* ReadTxPDO(uint16_t slave);

 private:
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

  uint8_t rxpdo_buffer_[80] = {0};
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
  std::mutex callback_mutex_;

  ProductConfig config_;

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
  std::string ReadFirmwareVersion(uint8_t mcu_id);

  // Static helper methods
  static void FoeProgressHook(uint16_t slave, int32_t packetnumber,
                              int32_t totalsize);

  // Parse PDO raw data and trigger structured callbacks
  void ParseHandAndJoints(const uint8_t* data, size_t size, size_t* offset,
                          std::vector<Joint>* parsed_joints,
                          HandState* parsed_temperature) const;
  TactileData ParseTactileData(const uint8_t* data, size_t size,
                               size_t* offset) const;
  void ParseAndNotify(const uint8_t* data, size_t size);
};

}  // namespace internal
}  // namespace ghand

#endif  // GHAND_INTERNAL_ETHERCAT_COMM_H_
