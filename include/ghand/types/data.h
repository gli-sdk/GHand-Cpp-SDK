// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef INCLUDE_GHAND_TYPES_DATA_H_
#define INCLUDE_GHAND_TYPES_DATA_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ghand/types/enums.h"
#include "ghand/types/errors.h"

namespace ghand {

// Device info structure
struct DeviceInfo {
  std::string device_name = "N/A";
  std::string software_version = "N/A";       // Main controller firmware version
  std::string position_sensor_version = "N/A";
  std::string tactile_sensor_version = "N/A";
  std::string motor_driver_version = "N/A";
  std::string firmware_package_version = "N/A";
  std::string thumb_tactile_sensor_version = "N/A";
  std::string finger_tactile_sensor_version = "N/A";
  std::string serial_number = "N/A";
};

/**
 * @brief Hand state data structure
 */
struct HandData {
  HandControlState joints_state = HandControlState::DISABLED;
  HandWarning error_warn;
  HandFault error_fault;
  HandCriticalError error_critical;
  int16_t temperature = 0;  // Back-of-hand temperature in degrees Celsius.
};

// One finger's tactile data, with forces in Newtons.
struct TactileData {
  bool state = false;
  std::vector<float> resultant_force;  // [Fx, Fy, Fz]; empty when unavailable.
  std::vector<float> distributed_force;  // Flattened triples; empty when unavailable.
};

// Joint command structure for parameterized joint control
struct JointCommand {
  JointCommand() = default;
  JointCommand(JointId joint_id,
               JointControlMode target_mode = JointControlMode::POSITION,
               JointLoopType target_loop = JointLoopType::CLOSED_LOOP,
               JointControlState target_state = JointControlState::RUNNING,
               float target_angle = 0.0f, int16_t target_velocity = 0,
               int16_t target_current = 0, int16_t target_kp = 0,
               int16_t target_kd = 0)
      : id(joint_id), control_mode(target_mode), loop_type(target_loop),
        control_state(target_state), angle(target_angle),
        velocity(target_velocity), current(target_current),
        kp(target_kp), kd(target_kd) {}
  JointId id = JointId::THUMB_IP;  // Joint identifier
  JointControlMode control_mode = JointControlMode::POSITION;
  JointLoopType loop_type = JointLoopType::CLOSED_LOOP;
  JointControlState control_state = JointControlState::RUNNING;
  float angle = 0.0f;              // Target angle (deg)
  int16_t velocity = 0;             // Target velocity (-100~100%, depending on control mode)
  int16_t current = 0;              // Target current (-100~100%, depending on control mode)
  int16_t kp = 0;                  // MIT proportional gain
  int16_t kd = 0;                  // MIT derivative gain
};

struct JointData {
  JointId id = JointId::THUMB_IP;
  JointState state = JointState::DISABLED;
  JointErrorCode error = JointErrorCode::NORMAL;
  JointControlMode control_mode = JointControlMode::POSITION;
  JointLoopType loop_type = JointLoopType::CLOSED_LOOP;
  float angle = 0.0f;
  int16_t velocity = 0;
  int16_t current = 0;
};

// Empty tactile data and a null timestamp indicate unavailable values.
struct DeviceData {
  HandData hand;
  std::vector<JointData> joints;
  std::map<TactileSensorId, TactileData> tactile;
  std::shared_ptr<const double> timestamp;
};

}  // namespace ghand

#endif  // INCLUDE_GHAND_TYPES_DATA_H_
