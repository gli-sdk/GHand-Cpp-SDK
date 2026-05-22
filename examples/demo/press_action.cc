#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// Thumb press pose
std::vector<ghand::JointCommand> MakeThumbPress() {
  return {
      {ghand::JointId::THUMB_PIP, 0.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 0.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 30.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 75.0f, 100, 100},
      {ghand::JointId::FF_MCP, 70.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
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
      {ghand::JointId::THUMB_PIP, 30.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 20.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, 0.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
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
      {ghand::JointId::THUMB_PIP, 60.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 0.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 75.0f, 100, 100},
      {ghand::JointId::FF_MCP, 70.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
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
      {ghand::JointId::THUMB_PIP, 66.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 0.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 75.0f, 100, 100},
      {ghand::JointId::FF_MCP, 70.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
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
      {ghand::JointId::THUMB_PIP, 66.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 0.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 75.0f, 100, 100},
      {ghand::JointId::FF_MCP, 70.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
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
      {ghand::JointId::THUMB_PIP, 0.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 0.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, 0.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, 0.0f, 100, 100},
  };
}

bool Press(ghand::DexHand& hand) {
  const std::vector<std::vector<ghand::JointCommand>> poses = {
      MakeThumbPress(), MakeIndexPress(), MakeMiddlePress(), MakeRingPress(),
      MakeLittlePress()};
  for (const auto& pose : poses) {
    if (!hand.MoveJoints(pose)) return false;
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  return true;
}

bool HandZero(ghand::DexHand& hand) {
  auto joints = MakeOpenHand();
  return hand.MoveJoints(joints);
}

int main() {
  std::cout << "***** Xiaoyao Dexterous Hand SDK - Press Demo *****\n" << '\n';
  auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                      ghand::CommType::ETHERCAT);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << '\n';
    return -1;
  }
  bool connected = hand->Connect("auto");
  if (!connected) {
    std::cout << "\n[Scan complete] Failed to connect to dexterous hand."
              << '\n';
    return 1;
  }
  std::cout << "\n--- Device ready, starting press demo ---\n" << '\n';

  int gesture_cycle = 0;
  const int max_cycles = 0;

  while (true) {
    gesture_cycle++;
    if (max_cycles > 0 && gesture_cycle > max_cycles) break;

    std::cout << "\n--- Round " << gesture_cycle << " demo started ---"
              << '\n';

    if (!Press(*hand)) {
      std::cout << "Round " << gesture_cycle << " press action execution failed"
                << '\n';
      break;
    }

    if (!HandZero(*hand)) {
      std::cout << "Round " << gesture_cycle << " reset action execution failed"
                << '\n';
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "--- Round " << gesture_cycle << " demo finished ---\n"
              << '\n';

    if (max_cycles == 0) {
      std::cout << "Press Ctrl+C to stop demo and exit\n" << '\n';
    }
  }

  hand->Disconnect();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "\n--- Demo finished, disconnecting ---" << '\n';
  return 0;
}
