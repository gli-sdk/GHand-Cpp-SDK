// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// Print a short subset of the feedback so the sample output stays readable.
void PrintJointStates(const std::vector<ghand::JointData>& joints) {
  for (const auto& joint : joints) {
    if (joint.id != ghand::JointId::THUMB_MCP &&
        joint.id != ghand::JointId::FF_PIP &&
        joint.id != ghand::JointId::MF_PIP) {
      continue;
    }

    std::cout << "  " << std::left << std::setw(15)
              << ghand::ToString(joint.id)
              << "- state: " << ghand::ToString(joint.state)
              << ", error: " << ghand::ToString(joint.error)
              << ", angle: " << std::fixed << std::setprecision(2)
              << joint.angle << " deg"
              << ", speed: " << static_cast<int>(joint.velocity)
              << ", current: " << static_cast<int>(joint.current) << '\n';
  }
}

int main() {
  auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create GHand" << '\n';
    return -1;
  }

  std::cout << "Connecting to dexterous hand..." << '\n';
  const auto connected = master->Connect("auto");
  if (!connected) {
    std::cerr << "Failed to connect to the dexterous hand: "
              << connected.message << '\n';
    return 1;
  }
  ghand::GHand* hand = connected.value.front();

  // CURRENT mode only uses `current`; `angle` and `velocity` are ignored.
  // Positive current closes the fingers and negative opens them. This demo
  // drives the thumb and the PIP/MCP joints of the four fingers.
  std::vector<ghand::JointCommand> joints = {
      {ghand::JointId::THUMB_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 0, 10},
      {ghand::JointId::THUMB_TMC_FE, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 0, 10},
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 0, 10},
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 0, 10},
      {ghand::JointId::MF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 0, 10},
      {ghand::JointId::MF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 0, 10},
      {ghand::JointId::RF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 0, 10},
      {ghand::JointId::RF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 0, 10},
      {ghand::JointId::LF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 0, 10},
      {ghand::JointId::LF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 0, 10},
  };

  for (auto& joint : joints) joint.control_mode = ghand::JointControlMode::CURRENT;

  std::cout << "Closing fingers (current=10)..." << '\n';
  const auto close_result =
      hand->MoveJoints(joints, ghand::HandControlMode::CURRENT);
  if (!close_result) {
    std::cerr << "Failed to send current command: " << close_result.message
              << '\n';
    master->Disconnect();
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  std::cout << "Current joint states:" << '\n';
  const auto joint_data = hand->GetJointsData();
  if (joint_data) {
    PrintJointStates(joint_data.value);
  } else {
    std::cerr << "Failed to read joint data: " << joint_data.message << '\n';
  }

  for (auto& joint : joints) {
    joint.current = -10;
  }

  std::cout << "Opening fingers (current=-10)..." << '\n';
  const auto open_result = hand->MoveJoints(joints, ghand::HandControlMode::CURRENT);
  if (!open_result) {
    std::cerr << "Failed to send current command: " << open_result.message
              << '\n';
    master->Disconnect();
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  std::cout << "Current joint states:" << '\n';
  const auto opened_data = hand->GetJointsData();
  if (opened_data) {
    PrintJointStates(opened_data.value);
  } else {
    std::cerr << "Failed to read joint data: " << opened_data.message << '\n';
  }

  const auto stop_result = hand->Stop();
  if (!stop_result) {
    std::cerr << "Failed to stop: " << stop_result.message << '\n';
  }
  const auto disconnected = master->Disconnect();
  if (!disconnected) {
    std::cerr << "Disconnect failed: " << disconnected.message << '\n';
    return 1;
  }
  std::cout << "Disconnected." << '\n';
  return 0;
}
