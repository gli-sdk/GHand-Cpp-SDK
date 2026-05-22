#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// Lift pose
std::vector<ghand::JointCommand> MakeLiftPose() {
  return {
      {ghand::JointId::THUMB_PIP, 0.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 0.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 60.0f, 100, 100},
      {ghand::JointId::FF_MCP, 35.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 60.0f, 100, 100},
      {ghand::JointId::MF_MCP, 35.0f, 100, 100},
      {ghand::JointId::RF_PIP, 60.0f, 100, 100},
      {ghand::JointId::RF_MCP, 35.0f, 100, 100},
      {ghand::JointId::LF_PIP, 60.0f, 100, 100},
      {ghand::JointId::LF_MCP, 35.0f, 100, 100},
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

bool Lift(ghand::DexHand& hand) {
  auto joints = MakeLiftPose();
  return hand.MoveJoints(joints);
}

bool HandZero(ghand::DexHand& hand) {
  auto joints = MakeOpenHand();
  return hand.MoveJoints(joints);
}

int main() {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
#endif
  std::cout << "***** Xiaoyao Dexterous Hand SDK - Lift Demo *****\n"
            << std::endl;
  auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                      ghand::CommType::ETHERCAT);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << std::endl;
    return -1;
  }
  bool connected = hand->Connect("auto");
  if (!connected) {
    std::cout << "\n[Scan complete] Failed to connect to dexterous hand."
              << std::endl;
    return 1;
  }
  std::cout << "\n--- Device ready, starting lift demo ---\n" << std::endl;

  int gesture_cycle = 0;
  const int max_cycles = 0;

  while (true) {
    gesture_cycle++;
    if (max_cycles > 0 && gesture_cycle > max_cycles) break;

    std::cout << "\n--- Round " << gesture_cycle << " demo started ---"
              << std::endl;

    if (!Lift(*hand)) {
      std::cout << "Round " << gesture_cycle << " lift action execution failed"
                << std::endl;
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));

    if (!HandZero(*hand)) {
      std::cout << "Round " << gesture_cycle << " reset action execution failed"
                << std::endl;
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << "--- Round " << gesture_cycle << " demo finished ---\n"
              << std::endl;

    if (max_cycles == 0) {
      std::cout << "Press Ctrl+C to stop demo and exit\n" << std::endl;
    }
  }

  hand->Disconnect();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "\n--- Demo finished, disconnecting ---" << std::endl;
  return 0;
}
