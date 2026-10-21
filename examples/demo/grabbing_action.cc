#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// Grab pose
std::vector<ghand::JointCommand> MakeGrabPose() {
  return {
      {ghand::JointId::THUMB_MCP, 50.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 40.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 59.0f, 100, 100},
      {ghand::JointId::FF_MCP, 69.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 60.0f, 100, 100},
      {ghand::JointId::MF_MCP, 56.0f, 100, 100},
      {ghand::JointId::RF_PIP, 60.0f, 100, 100},
      {ghand::JointId::RF_MCP, 54.0f, 100, 100},
      {ghand::JointId::LF_PIP, 62.0f, 100, 100},
      {ghand::JointId::LF_MCP, 64.0f, 100, 100},
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

bool Grab(ghand::Master& master) {
  auto joints = MakeGrabPose();
  return master.MoveJoints(joints);
}

bool HandZero(ghand::Master& master) {
  auto joints = MakeOpenHand();
  return master.MoveJoints(joints);
}

int main() {
  std::cout << "***** GHand SDK - Grabbing Demo *****\n"
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
  std::cout << "\n--- Device ready, starting grabbing demo ---\n" << '\n';

  int gesture_cycle = 0;
  const int max_cycles = 0;

  while (true) {
    gesture_cycle++;
    if (max_cycles > 0 && gesture_cycle > max_cycles) break;

    std::cout << "\n--- Round " << gesture_cycle << " demo started ---"
              << '\n';

    if (!Grab(*master)) {
      std::cout << "Round " << gesture_cycle << " grab action execution failed"
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
