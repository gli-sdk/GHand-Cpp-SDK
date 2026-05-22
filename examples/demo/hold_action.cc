#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// Hold pose
std::vector<ghand::JointCommand> MakeHoldPose() {
  return {
      {ghand::JointId::THUMB_PIP, 10.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 25.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 30.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 70.0f, 100, 100},
      {ghand::JointId::FF_MCP, 65.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 70.0f, 100, 100},
      {ghand::JointId::MF_MCP, 65.0f, 100, 100},
      {ghand::JointId::RF_PIP, 70.0f, 100, 100},
      {ghand::JointId::RF_MCP, 60.0f, 100, 100},
      {ghand::JointId::LF_PIP, 70.0f, 100, 100},
      {ghand::JointId::LF_MCP, 65.0f, 100, 100},
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

bool Hold(ghand::DexHand& hand) {
  auto joints = MakeHoldPose();
  return hand.MoveJoints(joints);
}

bool HandZero(ghand::DexHand& hand) {
  auto joints = MakeOpenHand();
  return hand.MoveJoints(joints);
}

int main() {
  std::cout << "***** 枭尧灵巧手 SDK - 握功能演示 *****\n" << std::endl;
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
  std::cout << "\n--- Device ready, starting hold demo ---\n" << std::endl;

  int gesture_cycle = 0;
  const int max_cycles = 0;

  while (true) {
    gesture_cycle++;
    if (max_cycles > 0 && gesture_cycle > max_cycles) break;

    std::cout << "\n--- Round " << gesture_cycle << " demo started ---"
              << std::endl;

    if (!Hold(*hand)) {
      std::cout << "第 " << gesture_cycle << " 轮演示中的握紧动作执行失败"
                << std::endl;
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));

    if (!HandZero(*hand)) {
      std::cout << "第 " << gesture_cycle << " 轮演示中的复位动作执行失败"
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
