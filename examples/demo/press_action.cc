// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// Thumb press pose
std::vector<ghand::JointCommand> MakeThumbPress() {
  return {
      {ghand::JointId::THUMB_MCP, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 75.0f, 100, 100},
      {ghand::JointId::FF_MCP, 70.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 75.0f, 100, 100},
      {ghand::JointId::MF_MCP, 70.0f, 100, 100},
      {ghand::JointId::RF_PIP, 75.0f, 100, 100},
      {ghand::JointId::RF_MCP, 70.0f, 100, 100},
      {ghand::JointId::LF_PIP, 74.0f, 100, 100},
      {ghand::JointId::LF_MCP, 70.0f, 100, 100},
  };
}

// Index finger press pose
std::vector<ghand::JointCommand> MakeIndexPress() {
  return {
      {ghand::JointId::THUMB_MCP, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 75.0f, 100, 100},
      {ghand::JointId::MF_MCP, 70.0f, 100, 100},
      {ghand::JointId::RF_PIP, 75.0f, 100, 100},
      {ghand::JointId::RF_MCP, 70.0f, 100, 100},
      {ghand::JointId::LF_PIP, 74.0f, 100, 100},
      {ghand::JointId::LF_MCP, 70.0f, 100, 100},
  };
}

// Middle finger press pose
std::vector<ghand::JointCommand> MakeMiddlePress() {
  return {
      {ghand::JointId::THUMB_MCP, 60.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 75.0f, 100, 100},
      {ghand::JointId::FF_MCP, 70.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, 75.0f, 100, 100},
      {ghand::JointId::RF_MCP, 70.0f, 100, 100},
      {ghand::JointId::LF_PIP, 74.0f, 100, 100},
      {ghand::JointId::LF_MCP, 70.0f, 100, 100},
  };
}

// Ring finger press pose
std::vector<ghand::JointCommand> MakeRingPress() {
  return {
      {ghand::JointId::THUMB_MCP, 66.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 75.0f, 100, 100},
      {ghand::JointId::FF_MCP, 70.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 75.0f, 100, 100},
      {ghand::JointId::MF_MCP, 70.0f, 100, 100},
      {ghand::JointId::RF_PIP, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, 74.0f, 100, 100},
      {ghand::JointId::LF_MCP, 70.0f, 100, 100},
  };
}

// Little finger press pose
std::vector<ghand::JointCommand> MakeLittlePress() {
  return {
      {ghand::JointId::THUMB_MCP, 66.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 75.0f, 100, 100},
      {ghand::JointId::FF_MCP, 70.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 75.0f, 100, 100},
      {ghand::JointId::MF_MCP, 70.0f, 100, 100},
      {ghand::JointId::RF_PIP, 75.0f, 100, 100},
      {ghand::JointId::RF_MCP, 70.0f, 100, 100},
      {ghand::JointId::LF_PIP, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, 0.0f, 100, 100},
  };
}

// Open hand pose
std::vector<ghand::JointCommand> MakeOpenHand() {
  return {
      {ghand::JointId::THUMB_MCP, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, 0.0f, 100, 100},
  };
}

bool Press(ghand::Master& master) {
  const std::vector<std::vector<ghand::JointCommand>> poses = {
      MakeThumbPress(), MakeIndexPress(), MakeMiddlePress(), MakeRingPress(),
      MakeLittlePress()};
  for (const auto& pose : poses) {
    if (!master.MoveJoints(pose)) return false;
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  return true;
}

bool ResetToOpenHand(ghand::Master& master) {
  auto joints = MakeOpenHand();
  return master.MoveJoints(joints);
}

int main() {
  std::cout << "***** GHand SDK - Press Demo *****\n\n";
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
  std::cout << "\n--- Device ready, starting press demo ---\n";
  std::cout << "Press Ctrl+C to stop demo and exit\n\n";

  bool failed = false;
  int round = 0;
  while (true) {
    ++round;
    std::cout << "\n--- Round " << round << " demo started ---\n";

    if (!Press(*master)) {
      std::cerr << "Round " << round << " press action execution failed\n";
      failed = true;
      break;
    }

    if (!ResetToOpenHand(*master)) {
      std::cerr << "Round " << round << " reset action execution failed\n";
      failed = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "--- Round " << round << " demo finished ---\n";
  }

  if (failed) {
    const auto stopped = master->Stop();
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
