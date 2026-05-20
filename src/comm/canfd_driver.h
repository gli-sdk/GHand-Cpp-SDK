#ifndef SRC_INTERNAL_CANFD_DRIVER_H_
#define SRC_INTERNAL_CANFD_DRIVER_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "canfd_protocol.h"

namespace ghand {
namespace internal {

/**
 * @brief 平台无关的 CANFD 驱动接口
 *
 * 由 ZLG 驱动实现（Windows/Linux 统一）或平台特定实现。
 */
class CANFDDriver {
 public:
  virtual ~CANFDDriver() = default;

  /**
   * @brief 打开 CANFD 通道
   * @param name 通道名称（如 "can0" 或 "PCAN_USBBUS1"）
   * @param bitrate 仲裁段波特率（如 1000000）
   * @param dbitrate 数据段波特率（如 5000000）
   * @return 0 成功，负值失败
   */
  virtual int Open(const std::string& name, uint32_t bitrate,
                   uint32_t dbitrate) = 0;

  /**
   * @brief 关闭驱动
   */
  virtual void Close() = 0;

  /**
   * @brief 发送单帧
   * @param frame CANFD 帧
   * @return 0 成功，负值失败
   */
  virtual int Send(const canfd::Frame& frame) = 0;

  /**
   * @brief 接收单帧（阻塞）
   * @param frame 输出帧
   * @param timeout_ms 超时时间（毫秒）
   * @return 0 成功，负值失败或超时
   */
  virtual int Receive(canfd::Frame& frame, int timeout_ms) = 0;

  /**
   * @brief 枚举可用适配器
   * @return 适配器名称到描述的映射
   */
  virtual std::map<std::string, std::string> EnumerateAdapters() = 0;

  /**
   * @brief 检查是否已打开
   */
  virtual bool IsOpen() const = 0;
};

// 工厂函数：创建平台相关的 CANFD 驱动实例
std::unique_ptr<CANFDDriver> CreateZLGDriver();

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_CANFD_DRIVER_H_
