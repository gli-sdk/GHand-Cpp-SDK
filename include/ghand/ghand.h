// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef INCLUDE_GHAND_GHAND_H_
#define INCLUDE_GHAND_GHAND_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ghand/export.h"
#include "ghand/types.h"
#include "ghand/version.h"

namespace ghand {

// Forward declaration of internal implementation class
namespace internal {
class GHandImpl;
}  // namespace internal

// Callback type definitions
using JointsCallback = std::function<void(const std::vector<Joint>&)>;
using HandStateCallback = std::function<void(const HandState&)>;
using TactileDataCallback = std::function<void(const TactileData&)>;

/**
 * @brief GHand robotic hand control interface
 *
 * This is the sole public API class, providing full control of the robotic
 * hand.
 */
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
class GHAND_API GHand {
 public:
  static std::unique_ptr<GHand> Create(
      ProductType product_type = ProductType::AUTO,
      CommType comm_type = CommType::ETHERCAT);
  ~GHand();

  // GHand owns device state and is not copyable or movable.
  GHand(const GHand&) = delete;
  GHand& operator=(const GHand&) = delete;
  GHand(GHand&&) = delete;
  GHand& operator=(GHand&&) = delete;

  // Connection management
  /**
   * @brief Auto-connect to device
   * @details Searches all adapters until a matching product connects. If none
   * connects, reports the first PRODUCT_MISMATCH, when present, and preserves
   * that device's information for GetDeviceInfo().
   * @return true on success; call GetLastResult() when false
   */
  bool AutoConnect();

  /**
   * @brief Connect to specified device.
   * @param device_name Device name ("auto" for auto-discovery)
   * @param slave_id RS485/CANFD slave ID used for this connection; 0 scans
   *     default IDs 0x31 and 0x32
   * @return true on success; call GetLastResult() when false
   */
  bool Connect(const std::string& device_name = "auto",
               uint8_t slave_id = 0);

  /**
   * @brief Connect to an RS485 device using a typed baud-rate selection.
   * @param device_name Device name ("auto" for auto-discovery)
   * @param slave_id RS485 slave ID used for this connection; 0 scans default
   *     IDs 0x31 and 0x32
   * @param baud_rate RS485 baud rate used for this connection
   * @return true on success; call GetLastResult() when false
   */
  bool Connect(const std::string& device_name, uint8_t slave_id,
               RS485BaudRate baud_rate);

  /**
   * @brief Connect to a CAN-FD device using a typed bit-timing profile.
   * @param device_name Device name ("auto" for auto-discovery)
   * @param slave_id CAN-FD slave ID used for this connection; 0 scans default
   *     IDs 0x31 and 0x32
   * @param timing CAN-FD bit timing profile used for this connection
   * @return true on success; call GetLastResult() when false
   */
  bool Connect(const std::string& device_name, uint8_t slave_id,
               CANFDBitTiming timing);

  /**
   * @brief Disconnect
   */
  bool Disconnect();

  /**
   * @brief Check if connected
   */
  bool IsConnected() const;

  /**
   * @brief Set RS485/CANFD slave ID on the connected device.
   * @param slave_id New slave ID written to holding register 0x0000
   * @return true on success; call GetLastResult() when false
   */
  bool SetSlaveId(uint8_t slave_id);

  /**
   * @brief Set RS485 baud-rate on the connected device.
   *
   * The mapped protocol gear is written to holding register 0x002C, saved by
   * the device, and takes effect after the next power-up.
   *
   * @param baud_rate RS485 baud rate
   * @return true on success; call GetLastResult() when false
   */
  bool SetBaudrateConfig(RS485BaudRate baud_rate);

  /**
   * @brief Set CAN-FD bit timing on the connected device.
   *
   * The mapped protocol gear is written to holding register 0x002C, saved by
   * the device, and takes effect after the next power-up.
   *
   * @param timing CAN-FD bit timing profile
   * @return true on success; call GetLastResult() when false
   */
  bool SetBaudrateConfig(CANFDBitTiming timing);

  /**
   * @brief Open a CAN-FD adapter at the requested timing without handshaking.
   *
   * Use this after writing a new CAN-FD baud-rate configuration: keep the
   * adapter open at the new timing, power-cycle the device, then reconnect
   * with the same timing.
   *
   * @param device_name Device name ("auto" for auto-discovery)
   * @param timing CAN-FD bit timing profile to listen on
   * @return true on success; call GetLastResult() when false
   */
  bool PrepareCANFDBaudrateListener(
      const std::string& device_name = "auto",
      CANFDBitTiming timing = CANFDBitTiming::TIMING_1M_5M);

  /**
   * @brief Return the most recent SDK operation result.
   *
   * This is a compatibility extension for bool-returning APIs. The result is
   * updated by core connection and control operations and is safe to query from
   * user callbacks.
   */
  OperationResult GetLastResult() const;

  /**
   * @brief Return the most recent SDK error code.
   */
  SdkError GetLastError() const;

  /**
   * @brief Return the most recent SDK error message.
   */
  std::string GetLastErrorMessage() const;

  /**
   * @brief Return a structured SDK state snapshot for diagnostics.
   *
   * Diagnostics are machine-readable state and do not depend on log parsing.
   * Transport counters are reserved for protocol-level instrumentation.
   */
  Diagnostics GetDiagnostics() const;

  // Device info
  /**
   * @brief Search available communication adapters
   * @return Adapter name mapping table
   */
  std::map<std::string, std::string> SearchAdapters();

  /**
   * @brief Get hand type
   */
  HandType GetHandType();

  /**
   * @brief Get device info
   * @details While disconnected, returns the information read during the last
   * connection attempt, including after PRODUCT_MISMATCH. A new connection
   * attempt clears this information. Recreate the SDK with the actual product
   * type before reconnecting to a mismatched device.
   */
  DeviceInfo GetDeviceInfo();

  // Motion control
  /**
   * @brief Set control mode
   */
  void SetControlMode(ControlMode mode);

  /**
   * @brief Control joint movement
   * @param joints Joint command list, angle in degrees (deg)
   * @return true on success; call GetLastResult() when false
   */
  bool MoveJoints(const std::vector<JointCommand>& joints);

  /**
   * @brief Stop all motion immediately
   */
  void Stop();

  /**
   * @brief Clear fault state
   */
  bool ClearFault();

  /**
   * @brief Initialize joints
   */
  bool InitJoint();

  // Tactile sensor
  /**
   * @brief Open tactile sensor
   */
  bool OpenTactile();

  /**
   * @brief Close tactile sensor
   */
  bool CloseTactile();

  /**
   * @brief Zero tactile sensor
   */
  bool ZeroTactile();

  // Data access
  /**
   * @brief Get hand state (latest cached data, no hardware read triggered)
   */
  HandState GetHandData();

  /**
   * @brief Get joint data (latest cached data, no hardware read triggered)
   */
  std::vector<Joint> GetJointsData();

  /**
   * @brief Get tactile data (latest cached data, no hardware read triggered)
   */
  TactileData GetTactileData();

  /**
   * @brief Read structured self-test error information from the device.
   *
   * Reads only the already-latched self-test result stored on the device
   * (object dictionary ``0x2008``). This method does NOT trigger a manual
   * self-test or manual zeroing. Only categories flagged by the A0 summary
   * are queried in detail: Motor reads A8, Sensor reads A2, Joint reads A6.
   */
  SelfTestErrorInfo GetSelfTestErrorInfo();

  /**
   * @brief Read the current self-test status byte.
   *
   * EtherCAT reads 0x2008:0x02. CANFD and RS485 read the low byte of holding
   * register 0x0038.
   *
   * Status values: 0 idle, 1 processing, 2 command processed successfully,
   * 3 failed.
   */
  uint8_t GetSelfTestStatus();

  // Callback registration
  /**
   * @brief Register joint data callback
   * @param cb Callback function
   */
  void SetJointsCallback(JointsCallback cb);

  /**
   * @brief Register hand state callback
   * @param cb Callback function
   */
  void SetHandStateCallback(HandStateCallback cb);

  /**
   * @brief Register tactile data callback
   * @param cb Callback function
   */
  void SetTactileDataCallback(TactileDataCallback cb);

  // Firmware update
  FirmwareUpdateError BootUpdate(
      const std::string& filename,
      std::function<void(int)> progress_callback);

 private:
  GHand(ProductType product_type, CommType comm_type);
  std::unique_ptr<internal::GHandImpl> impl_;
};

}  // namespace ghand

#ifdef _WIN32
#pragma warning(pop)
#endif

#endif  // INCLUDE_GHAND_GHAND_H_
