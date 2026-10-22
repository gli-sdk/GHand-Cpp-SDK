// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// Joint status display callback. It runs on the communication thread; this
// demo only prints, so it needs no locking. A callback registered on one hand
// only ever receives that hand's data.
void DisplayJoints(const std::vector<ghand::Joint>& joints) {
  std::cout << "\n========== Joint Status ==========" << '\n';
  std::cout << std::left << std::setw(20) << "Joint" << std::setw(15)
            << "Angle(deg)" << std::setw(15) << "Velocity(%)" << std::setw(15)
            << "Current(%)" << std::setw(15) << "State"
            << "Error" << '\n';
  std::cout << std::string(80, '-') << '\n';

  for (const auto& joint : joints) {
    std::cout << std::left << std::setw(20) << ghand::ToString(joint.id)
              << std::fixed << std::setprecision(1) << std::setw(15)
              << joint.angle << std::setw(15) << +joint.velocity
              << std::setw(15) << +joint.current << std::setw(15)
              << ghand::ToString(joint.state) << ghand::ToString(joint.error)
              << '\n';
  }
  std::cout << "==================================" << '\n';
}

int main() {
  auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create GHand" << '\n';
    return -1;
  }

  // Try to connect to the dexterous hand
  std::cout << "Connecting to dexterous hand..." << '\n';
  const auto connected = master->Connect("auto");
  if (!connected) {
    std::cout << "Failed to connect: " << connected.message << '\n';
    return 1;
  }

  std::cout << "Successfully connected to the dexterous hand!" << '\n';
  ghand::GHand* hand = connected.value.front();

  // Register joint status callback to display joint data in real time
  hand->SetJointsCallback(DisplayJoints);
  std::cout << "Joint display callback registered." << '\n';

  // JointCommand's four constructor arguments are:
  //   id       joint to move
  //   angle    target angle in degrees (clamped to the configured range)
  //   velocity velocity in percent (-100 to 100); target in SPEED mode
  //   current  current in percent (-100 to 100); target in CURRENT mode
  //
  // MoveJoints(joints, mode) interprets the fields by mode:
  //   POSITION (default): angle is the target; velocity/current are limits
  //   SPEED:              velocity is the target
  //   CURRENT:            current is the target
  //   MIT:                angle is the target with the kp/kd gains
  //   CUSTOM:             each JointCommand.mode applies; INHERIT keeps the
  //                       previous mode
  //
  // The remaining fields keep their defaults: mode = INHERIT, stop = 1 (run),
  // kp = 0 and kd = 0 (MIT gains). See examples 03 and 04 for SPEED/CURRENT.
  //
  // This demo only commands the index finger. Joints that are not listed keep
  // their previous command on EtherCAT; on Modbus transports omitted joints
  // are sent as stopped entries. DIP joints are not controllable.
  const std::vector<ghand::JointCommand> joints = {
      // {id, target angle (deg), speed limit (%), current limit (%)}
      {ghand::JointId::FF_MCP, 30.0f, 100, 100},
      {ghand::JointId::FF_PIP, 45.0f, 100, 100},
  };

  std::cout << "Moving joints..." << '\n';
  // MoveJoints(joints, mode): mode defaults to POSITION; passing it explicitly
  // makes the interpretation of angle/velocity/current clear.
  const auto move_result =
      hand->MoveJoints(joints, ghand::ControlMode::POSITION);
  if (move_result) {
    std::cout << "Joints moved successfully!" << '\n';

    // Hold pose for 5 seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Reset joint positions
    const std::vector<ghand::JointCommand> reset_joints = {
        {ghand::JointId::FF_MCP, 0.0f, 100, 100},
        {ghand::JointId::FF_PIP, 0.0f, 100, 100}};

    std::cout << "Resetting joint positions..." << '\n';
    const auto reset_result =
        hand->MoveJoints(reset_joints, ghand::ControlMode::POSITION);
    if (!reset_result) {
      std::cerr << "Failed to reset joints: " << reset_result.message << '\n';
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
  } else {
    std::cerr << "Failed to move joints: " << move_result.message << '\n';
  }

  master->Disconnect();
  std::cout << "Connection closed." << '\n';

  return 0;
}
