#ifndef SRC_INTERNAL_RS485_COMM_H_
#define SRC_INTERNAL_RS485_COMM_H_

#include <atomic>
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
// Forward declare libmodbus context
struct modbus;
typedef struct modbus modbus_t;
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
  bool WriteTactileControl(uint16_t command);
  void EnsurePollStarted();
  void StopPoll();
  void PollLoop();

  std::vector<uint8_t> ReadInputRegistersBytes(int addr, int count);
  std::vector<Joint> GetJoints();
  HandState GetHandInfo();
  TactileData GetTactileData();

  modbus_t* ctx_ = nullptr;
  uint8_t slave_id_ = 0x31;
  std::atomic<bool> connected_{false};

  std::thread poll_thread_;
  std::atomic<bool> poll_running_{false};
  std::atomic<bool> poll_stop_{false};

  ProductConfig config_;

  // Callbacks
  JointsCallback joints_cb_;
  HandStateCallback hand_state_cb_;
  TactileDataCallback tactile_cb_;
  std::mutex cb_mutex_;
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_RS485_COMM_H_
