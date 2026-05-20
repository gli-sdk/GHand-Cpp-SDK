#ifndef SRC_INTERNAL_CANFD_COMM_H_
#define SRC_INTERNAL_CANFD_COMM_H_

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "canfd_driver.h"
#include "canfd_protocol.h"
#include "icomm.h"
#include "product_config.h"

namespace ghand {
namespace internal {

/**
 * @brief CANFD 通信实现类
 *
 * 实现 IComm 接口，使用自定义 CANFD 协议与设备通信。
 */
class CANFDComm : public IComm {
 public:
  explicit CANFDComm(const ProductConfig& config);
  ~CANFDComm() override;

  // IComm 接口实现
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

  int BootUpdate(const std::string& device_name, uint16_t slave,
                 const std::string& filename,
                 std::function<void(int)> progress) override;

 private:
  bool SendRecvCmd(canfd::FunctionCode fc, const uint8_t* param,
                   uint8_t param_len, std::vector<uint8_t>* response,
                   int timeout_ms = 500);
  bool SendCmdOnly(canfd::FunctionCode fc, const uint8_t* param,
                   uint8_t param_len);
  bool SendSingleFrame(const uint8_t* data, uint8_t total_len);
  bool SendMultiFrame(const uint8_t* data, uint8_t total_len);

  void ReceiveThread();
  void ProcessActiveReport(const std::vector<uint8_t>& payload,
                           canfd::FunctionCode fc);

  bool SubscribeActiveReport(canfd::FunctionCode fc, uint8_t period_ms);
  bool UnsubscribeActiveReport(canfd::FunctionCode fc);

  void ParseJointStates(const uint8_t* data, size_t len);
  void ParseTactileForce(const uint8_t* data, size_t len);
  void ParseHandError(const uint8_t* data, size_t len);

  float RawToAngle(uint16_t raw, JointId id);
  uint16_t AngleToRaw(float angle_deg, JointId id);

 private:
  std::unique_ptr<CANFDDriver> driver_;
  uint8_t device_id_ = 0x00;
  std::atomic<bool> connected_{false};

  std::thread rx_thread_;
  std::atomic<bool> rx_running_{false};

  const ProductConfig& config_;

  // 请求-响应匹配（按功能码分槽，支持多请求并发）
  struct ResponseSlot {
    std::vector<uint8_t> payload;
    bool ready = false;
  };
  std::mutex response_mutex_;
  std::condition_variable response_cv_;
  std::map<uint8_t, ResponseSlot> response_slots_;

  // 组包器（按 device_id + FrameType 隔离 RESPONSE / ACTIVE_REPORT）
  std::mutex asm_mutex_;
  std::map<std::pair<uint8_t, canfd::FrameType>, canfd::PacketAssembler>
      assemblers_;

  // 回调
  JointsCallback joints_cb_;
  HandStateCallback hand_state_cb_;
  TactileDataCallback tactile_cb_;
  std::mutex cb_mutex_;
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_CANFD_COMM_H_
