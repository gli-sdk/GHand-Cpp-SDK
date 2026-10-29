// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef INCLUDE_GHAND_GHAND_H_
#define INCLUDE_GHAND_GHAND_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ghand/export.h"
#include "ghand/result.h"
#include "ghand/types.h"
#include "ghand/version.h"

namespace ghand {

// Forward declarations of internal implementation types
namespace internal {
class GHandImpl;
struct HandFactory;
}  // namespace internal

class Master;

// Callback type definitions. A callback is registered on one hand (see
// GHand::SetJointsCallback) and only receives that hand's data.
using JointsCallback = std::function<void(const std::vector<JointData>& joints)>;
using HandStateCallback = std::function<void(const HandData& state)>;
using TactileDataCallback = std::function<void(const std::map<TactileSensorId, TactileData>& data)>;

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief One dexterous hand on a connected bus.
 *
 * Hands belong to the Master that discovered them; user code only holds a
 * non-owning pointer. A GHand* stays valid from the Connect() that returned it
 * until the next Connect()/Disconnect() on that Master, or until the Master is
 * destroyed.
 *
 * Every action names its hand through the object itself, so no slave index is
 * passed to the calls.
 */
class GHAND_API GHand {
 public:
  ~GHand();

  GHand(const GHand&) = delete;
  GHand& operator=(const GHand&) = delete;
  GHand(GHand&&) = delete;
  GHand& operator=(GHand&&) = delete;

  // Identity, as reported when the hand was discovered.
  uint16_t GetSlaveIndex() const { return slave_index_; }
  HandType GetHandType() const { return hand_type_; }

  /**
   * @brief Re-read this hand's device info from the device (bus round-trip).
   */
  ResultOf<DeviceInfo> GetDeviceInfo();

  // Motion control
  /**
   * @brief Control joint movement on this hand.
   *
   * The new protocol uses degrees, velocity and current percentages, and MIT
   * gains. Modbus fills omitted joints with stopped entries; EtherCAT retains
   * their previous commands. CUSTOM inherits each hand's previous joint modes.
   *
   * @param joints JointData command list
   * @param mode How the command is interpreted. Defaults to POSITION.
   */
  Result MoveJoints(const std::vector<JointCommand>& joints,
                    HandControlMode mode = HandControlMode::POSITION);

  /**
   * @brief Stop this hand's motion immediately
   */
  Result Stop();

  /**
   * @brief Clear this hand's fault state
   */
  Result DisableHand();
  ResultOf<HandRuntimeState> GetHandStateMachine();
  // All 13 active joints, in protocol joint order; EtherCAT only.
  ResultOf<std::map<JointId, uint8_t>> GetStallTolerances();
  Result SetStallTolerances(const std::map<JointId, uint8_t>& values);
  // 39 uint16 values (13 active joints x Kp/Ki/Kd), in protocol joint order.
  // Supported on GHand5 over EtherCAT, RS485 and CANFD.
  ResultOf<std::vector<uint16_t>> GetPidParameters();
  Result SetPidParameters(const std::vector<uint16_t>& values);
  Result ClearFault();

  // Tactile sensor
  Result OpenTactile();
  Result CloseTactile();
  Result ZeroTactile();

  // Configuration
  /**
   * @brief Rename this hand's RS485/CANFD slave ID.
   *
   * Takes effect immediately but is not persisted: the device returns to the
   * default 0x31/0x32 after the next power-up.
   */
  Result SetSlaveId(uint8_t new_slave_id);

  /**
   * @brief Set this hand's RS485 baud rate (saved, effective after power-up)
   */
  Result SetBaudrateConfig(RS485BaudRate baud_rate);

  /**
   * @brief Set this hand's CAN-FD bit timing (saved, effective after power-up)
   */
  Result SetBaudrateConfig(CANFDBitTiming timing);

  // Data (push): the callback fires only for this hand.
  void SetJointsCallback(JointsCallback cb);
  // Data callbacks pause during power-on self-test and resume automatically.
  void SetHandStateCallback(HandStateCallback cb);
  void SetTactileDataCallback(TactileDataCallback cb);

  // Data (pull): latest cached data, no hardware read triggered.
  // All three return DEVICE_REJECTED while the internally cached runtime state is POWER_ON_SELF_TEST.
  ResultOf<HandData> GetHandData();
  ResultOf<std::vector<JointData>> GetJointsData();
  ResultOf<std::map<TactileSensorId, TactileData>> GetTactileData();

  /**
   * @brief Update this hand's firmware.
   * Currently requires EtherCAT with this hand as the only device at index 1.
   */
  FirmwareUpdateError BootUpdate(
      const std::string& filename,
      std::function<void(int)> progress_callback);

 private:
  friend class Master;
  friend struct internal::HandFactory;
  GHand(internal::GHandImpl* impl, uint16_t slave_index, HandType hand_type);

  internal::GHandImpl* impl_;
  uint16_t slave_index_;
  HandType hand_type_;
};

/**
 * @brief Session with one GHand bus.
 *
 * Owns the connection and the hands found on it.
 * Connection-level calls (connect, disconnect, discovery)
 * live here; everything a single hand does lives on GHand.
 */
class GHAND_API Master {
 public:
  static std::unique_ptr<Master> Create(
      ProductType product_type = ProductType::AUTO,
      CommType comm_type = CommType::ETHERCAT);
  ~Master();

  Master(const Master&) = delete;
  Master& operator=(const Master&) = delete;
  Master(Master&&) = delete;
  Master& operator=(Master&&) = delete;

  // Connection management
  /**
   * @brief Connect to a bus and enumerate the hands on it.
   * @param device_name Device name; "auto" probes every adapter and uses the
   *     first bus that carries a hand
   * @param slave_id RS485/CANFD slave ID used for this connection; 0 scans
   *     the default IDs 0x31 and 0x32 and manages every hand it finds
   * @return The hands found on the bus. Empty on failure, with the reason in
   *     code/message.
   */
  ResultOf<std::vector<GHand*>> Connect(const std::string& device_name = "auto",
                                        uint8_t slave_id = 0);

  /**
   * @brief Connect to an RS485 device using a typed baud-rate selection.
   */
  ResultOf<std::vector<GHand*>> Connect(const std::string& device_name,
                                        uint8_t slave_id,
                                        RS485BaudRate baud_rate);

  /**
   * @brief Connect to a CAN-FD device using a typed bit-timing profile.
   */
  ResultOf<std::vector<GHand*>> Connect(const std::string& device_name,
                                        uint8_t slave_id,
                                        CANFDBitTiming timing);

  Result Disconnect();
  bool IsConnected() const;

  // Discovery
  std::map<std::string, std::string> SearchAdapters();

  // Hands of the current connection; the list is the snapshot taken by the
  // Connect() that found them.
  std::vector<GHand*> hands() const;
  GHand* hand(HandType type) const;
  GHand* hand(uint16_t slave_index) const;

 private:
  Master(ProductType product_type, CommType comm_type);

  std::unique_ptr<internal::GHandImpl> impl_;
  std::vector<std::unique_ptr<GHand>> hands_;
};

}  // namespace ghand

#ifdef _WIN32
#pragma warning(pop)
#endif

#endif  // INCLUDE_GHAND_GHAND_H_
