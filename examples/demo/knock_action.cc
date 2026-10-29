// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// Knock pose
std::vector<ghand::JointCommand> MakeKnockPose() {
  return {
      {ghand::JointId::THUMB_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 60.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 75.0f, 100, 100},
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 75.0f, 100, 100},
      {ghand::JointId::MF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 75.0f, 100, 100},
      {ghand::JointId::RF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 70.0f, 100, 100},
      {ghand::JointId::LF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 74.0f, 100, 100},
      {ghand::JointId::LF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 70.0f, 100, 100},
  };
}

// Open hand pose
std::vector<ghand::JointCommand> MakeOpenHand() {
  return {
      {ghand::JointId::THUMB_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
  };
}

bool Knock(ghand::GHand& hand) {
  auto joints = MakeKnockPose();
  return hand.MoveJoints(joints);
}

bool ResetToOpenHand(ghand::GHand& hand) {
  auto joints = MakeOpenHand();
  return hand.MoveJoints(joints);
}

int main() {
  std::cout << "***** GHand SDK - Knock Demo *****\n\n";
  auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create GHand\n";
    return 1;
  }

  const auto connected = master->Connect("auto");
  if (!connected) {
    std::cerr << "Failed to connect to dexterous hand: " << connected.message
              << '\n';
    return 1;
  }
  auto* hand = connected.value.front();
  std::cout << "\n--- Device ready, starting knock demo ---\n";
  std::cout << "Press Ctrl+C to stop demo and exit\n\n";

  bool failed = false;
  int round = 0;
  while (true) {
    ++round;
    std::cout << "\n--- Round " << round << " demo started ---\n";

    if (!Knock(*hand)) {
      std::cerr << "Round " << round << " knock action execution failed\n";
      failed = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));

    if (!ResetToOpenHand(*hand)) {
      std::cerr << "Round " << round << " reset action execution failed\n";
      failed = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << "--- Round " << round << " demo finished ---\n";
  }

  if (failed) {
    const auto stopped = hand->Stop();
    if (!stopped) {
      std::cerr << "Stop failed: " << stopped.message << '\n';
    }
  }

  const auto disconnected = master->Disconnect();
  if (!disconnected) {
    std::cerr << "Disconnect failed: " << disconnected.message << '\n';
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "\n--- Demo finished, disconnected ---\n";
  return failed ? 1 : 0;
}
