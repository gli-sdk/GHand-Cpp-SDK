#ifndef SRC_INTERNAL_CANFD_DRIVER_H_
#define SRC_INTERNAL_CANFD_DRIVER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "canfd_protocol.h"

namespace ghand {
namespace internal {

/**
 * @brief Platform-independent CANFD driver interface
 *
 * Implemented by a platform-specific CANFD backend.
 */
class CANFDDriver {
 public:
  CANFDDriver() = default;
  virtual ~CANFDDriver() = default;

  CANFDDriver(const CANFDDriver&) = delete;
  CANFDDriver& operator=(const CANFDDriver&) = delete;
  CANFDDriver(CANFDDriver&&) = delete;
  CANFDDriver& operator=(CANFDDriver&&) = delete;

  /**
   * @brief Open CANFD channel
   * @param name Channel name (e.g. "can0" or "PCAN_USBBUS1")
   * @param bitrate Arbitration segment baud rate (e.g. 1000000)
   * @param dbitrate Data segment baud rate (e.g. 5000000)
   * @return 0 on success, negative value on failure
   */
  virtual int Open(const std::string& name, uint32_t bitrate,
                   uint32_t dbitrate) = 0;

  /**
   * @brief Close driver
   */
  virtual void Close() = 0;

  /**
   * @brief Send a single frame
   * @param frame CANFD frame
   * @return 0 on success, negative value on failure
   */
  virtual int Send(const canfd::Frame& frame) = 0;

  /**
   * @brief Receive a single frame (blocking)
   * @param frame Output frame
   * @param timeout_ms Timeout in milliseconds
   * @return 0 on success, negative value on failure or timeout
   */
  virtual int Receive(canfd::Frame* frame, int timeout_ms) = 0;

  /**
   * @brief Enumerate available adapters
   * @return Mapping of adapter name to description
   */
  virtual std::map<std::string, std::string> EnumerateAdapters() = 0;

  /**
   * @brief Check if the channel is open
   */
  virtual bool IsOpen() const = 0;
};

// Factory function: create the default CANFD driver instance.
std::unique_ptr<CANFDDriver> CreateCANFDDriver();

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_CANFD_DRIVER_H_
