// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// Gesture pose definitions.

// Thumb touches little finger
std::vector<ghand::JointCommand> MakeThumbTouchLittle() {
  return {
      {ghand::JointId::THUMB_MCP, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 50.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 60.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, 56.0f, 100, 100},
      {ghand::JointId::LF_MCP, 28.0f, 100, 100},
  };
}

// Thumb touches ring finger
std::vector<ghand::JointCommand> MakeThumbTouchRing() {
  return {
      {ghand::JointId::THUMB_MCP, 19.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 38.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 45.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, 67.0f, 100, 100},
      {ghand::JointId::RF_MCP, 39.0f, 100, 100},
      {ghand::JointId::LF_PIP, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, 0.0f, 100, 100},
  };
}

// Thumb touches middle finger
std::vector<ghand::JointCommand> MakeThumbTouchMiddle() {
  return {
      {ghand::JointId::THUMB_MCP, 17.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 27.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 40.0f, 100, 100},
      {ghand::JointId::MF_MCP, 57.0f, 100, 100},
      {ghand::JointId::RF_PIP, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, 0.0f, 100, 100},
  };
}

// Thumb touches index finger
std::vector<ghand::JointCommand> MakeThumbTouchIndex() {
  return {
      {ghand::JointId::THUMB_MCP, 13.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 14.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 51.0f, 100, 100},
      {ghand::JointId::FF_MCP, 46.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, 0.0f, 100, 100},
  };
}

// Fist pose
std::vector<ghand::JointCommand> MakeFist() {
  return {
      {ghand::JointId::THUMB_MCP, 40.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 65.0f, 100, 100},
      {ghand::JointId::FF_MCP, 55.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 65.0f, 100, 100},
      {ghand::JointId::MF_MCP, 55.0f, 100, 100},
      {ghand::JointId::RF_PIP, 65.0f, 100, 100},
      {ghand::JointId::RF_MCP, 55.0f, 100, 100},
      {ghand::JointId::LF_PIP, 65.0f, 100, 100},
      {ghand::JointId::LF_MCP, 55.0f, 100, 100},
  };
}

// Open index finger
std::vector<ghand::JointCommand> MakeOpenIndexFinger() {
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
      {ghand::JointId::LF_PIP, 70.0f, 100, 100},
      {ghand::JointId::LF_MCP, 70.0f, 100, 100},
  };
}

// Open middle finger
std::vector<ghand::JointCommand> MakeOpenMiddleFinger() {
  return {
      {ghand::JointId::THUMB_MCP, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, 75.0f, 100, 100},
      {ghand::JointId::RF_MCP, 70.0f, 100, 100},
      {ghand::JointId::LF_PIP, 70.0f, 100, 100},
      {ghand::JointId::LF_MCP, 70.0f, 100, 100},
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

// OK pose
std::vector<ghand::JointCommand> MakeOK() {
  return {
      {ghand::JointId::THUMB_MCP, 40.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 30.0f, 100, 100},
      {ghand::JointId::FF_MCP, 50.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, 0.0f, 100, 100},
  };
}

// Action execution functions.

bool ResetToOpenHand(ghand::Master& master) {
  auto joints = MakeOpenHand();
  return master.MoveJoints(joints);
}

bool ThumbTouch(ghand::Master& master) {
  const std::vector<std::vector<ghand::JointCommand>> poses = {
      MakeThumbTouchLittle(), MakeThumbTouchRing(), MakeThumbTouchMiddle(),
      MakeThumbTouchIndex()};
  for (const auto& pose : poses) {
    if (!master.MoveJoints(pose)) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return ResetToOpenHand(master);
}

bool FistThenOpen(ghand::Master& master) {
  if (!master.MoveJoints(MakeFist())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return ResetToOpenHand(master);
}

bool SeqOpenFinger(ghand::Master& master) {
  if (!master.MoveJoints(MakeFist())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (!master.MoveJoints(MakeOpenIndexFinger())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (!master.MoveJoints(MakeOpenMiddleFinger())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return ResetToOpenHand(master);
}

bool MakeOKGesture(ghand::Master& master) {
  if (!master.MoveJoints(MakeOK())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return ResetToOpenHand(master);
}

// First action group.
bool FirstAction(ghand::Master& master) {
  if (!ThumbTouch(master)) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  for (int i = 0; i < 2; i++) {
    if (!FistThenOpen(master)) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return true;
}

// Second action group.
bool SecondAction(ghand::Master& master) {
  if (!SeqOpenFinger(master)) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (!MakeOKGesture(master)) return false;
  return true;
}

int main() {
  std::cout << "***** GHand SDK - Gesture Dance Demo *****\n\n";
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
  std::cout << "\n--- Device ready, starting gesture dance demo ---\n";
  std::cout << "Press Ctrl+C to stop demo and exit\n\n";

  bool failed = false;
  int round = 0;
  while (true) {
    ++round;
    std::cout << "\n--- Round " << round << " gesture demo started ---\n";

    if (!FirstAction(*master)) {
      std::cerr << "Round " << round << " first action group failed\n";
      failed = true;
      break;
    }

    if (!SecondAction(*master)) {
      std::cerr << "Round " << round << " second action group failed\n";
      failed = true;
      break;
    }

    std::cout << "--- Round " << round << " gesture demo finished ---\n";
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
