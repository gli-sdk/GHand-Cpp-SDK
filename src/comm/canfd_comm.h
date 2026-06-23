#ifndef SRC_INTERNAL_CANFD_COMM_H_
#define SRC_INTERNAL_CANFD_COMM_H_

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
  int Connect(const std::string& device_name) override;
  int Disconnect() override;
  bool IsConnected() const override { return connected_.load(); }
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

 private:
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

  bool ReadRegisters(int addr, int count, std::vector<uint8_t>* out_bytes,
                     int func_code = 0x04, int timeout_ms = 500);
  bool WriteRegisters(int addr, const std::vector<uint8_t>& data,
                      int timeout_ms = 5000);

  bool WriteSingleRegister(int addr, uint16_t value, int timeout_ms = 500);

  bool NodeIdDetection(int timeout_ms = 500);
  bool EstablishConnection(int timeout_ms = 500);
  void DeleteConnection();

  bool ReadInputBytes(int addr, int count, std::vector<uint8_t>* bytes);
  std::vector<Joint> GetJoints();
  HandState GetHandInfo();
  TactileData GetTactileData();

  bool WriteTactileControl(uint16_t command);
  void EnsurePollStarted();
  void StopPoll();
  void PollLoop();

  std::unique_ptr<CANFDDriver> driver_;
  std::mutex io_mutex_;
  uint8_t src_id_ = 0x0A;
  uint8_t dst_id_ = 0x31;
  std::atomic<bool> connected_{false};

  std::thread poll_thread_;
  std::atomic<bool> poll_running_{false};
  std::atomic<bool> poll_stop_{false};

  ProductConfig config_;

  // Callbacks
  JointsCallback joints_cb_;
  HandStateCallback hand_state_cb_;
  TactileDataCallback tactile_cb_;
  bool joints_poll_enabled_ = false;
  bool hand_state_poll_enabled_ = false;
  bool tactile_poll_enabled_ = false;
  std::mutex cb_mutex_;
  std::map<JointId, JointCommand> command_cache_;
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_CANFD_COMM_H_
