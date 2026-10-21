#ifdef _WIN32
#include <windows.h>
#endif

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// Lift pose
std::vector<ghand::JointCommand> MakeLiftPose() {
  return {
      {ghand::JointId::THUMB_MCP, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 60.0f, 100, 100},
      {ghand::JointId::FF_MCP, 35.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
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

bool Lift(ghand::Master& master) {
  auto joints = MakeLiftPose();
  return master.MoveJoints(joints);
}

bool HandZero(ghand::Master& master) {
  auto joints = MakeOpenHand();
  return master.MoveJoints(joints);
}

int main() {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
#endif
  std::cout << "***** GHand SDK - Lift Demo *****\n"
            << '\n';
  auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create GHand" << '\n';
    return -1;
  }
  bool connected = master->Connect("auto");
  if (!connected) {
    std::cout << "\n[Scan complete] Failed to connect to dexterous hand."
              << '\n';
    return 1;
  }
  std::cout << "\n--- Device ready, starting lift demo ---\n" << '\n';

  int gesture_cycle = 0;
  const int max_cycles = 0;

  while (true) {
    gesture_cycle++;
    if (max_cycles > 0 && gesture_cycle > max_cycles) break;

    std::cout << "\n--- Round " << gesture_cycle << " demo started ---"
              << '\n';

    if (!Lift(*master)) {
      std::cout << "Round " << gesture_cycle << " lift action execution failed"
                << '\n';
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));

    if (!HandZero(*master)) {
      std::cout << "Round " << gesture_cycle
                << " reset action execution failed" << '\n';
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << "--- Round " << gesture_cycle << " demo finished ---\n"
              << '\n';

    if (max_cycles == 0) {
      std::cout << "Press Ctrl+C to stop demo and exit\n" << '\n';
    }
  }

  master->Disconnect();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "\n--- Demo finished, disconnecting ---" << '\n';
  return 0;
}
