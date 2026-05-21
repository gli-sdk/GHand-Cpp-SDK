#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

using ghand::DexHand;
\nusing ghand::JointCommand;
\nusing ghand::JointId;
\nusing ghand::ProductType;
\nusing ghand::CommType;
\n

    // Thumb press pose
    std::vector<JointCommand> MakeThumbPress() {
  return {
      {JointId::THUMB_PIP, 0.0f, 100, 100},
      {JointId::THUMB_MCP, 0.0f, 100, 100},
      {JointId::THUMB_SWING, 30.0f, 100, 100},
      {JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {JointId::FF_PIP, 75.0f, 100, 100},
      {JointId::FF_MCP, 70.0f, 100, 100},
      {JointId::FF_SWING, 0.0f, 100, 100},
      {JointId::MF_PIP, 75.0f, 100, 100},
      {JointId::MF_MCP, 70.0f, 100, 100},
      {JointId::RF_PIP, 75.0f, 100, 100},
      {JointId::RF_MCP, 70.0f, 100, 100},
      {JointId::LF_PIP, 74.0f, 100, 100},
      {JointId::LF_MCP, 70.0f, 100, 100},
  };
}

// Index finger press pose
std::vector<JointCommand> MakeIndexPress() {
  return {
      {JointId::THUMB_PIP, 30.0f, 100, 100},
      {JointId::THUMB_MCP, 20.0f, 100, 100},
      {JointId::THUMB_SWING, 20.0f, 100, 100},
      {JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {JointId::FF_PIP, 0.0f, 100, 100},
      {JointId::FF_MCP, 0.0f, 100, 100},
      {JointId::FF_SWING, 0.0f, 100, 100},
      {JointId::MF_PIP, 75.0f, 100, 100},
      {JointId::MF_MCP, 70.0f, 100, 100},
      {JointId::RF_PIP, 75.0f, 100, 100},
      {JointId::RF_MCP, 70.0f, 100, 100},
      {JointId::LF_PIP, 74.0f, 100, 100},
      {JointId::LF_MCP, 70.0f, 100, 100},
  };
}

// Middle finger press pose
std::vector<JointCommand> MakeMiddlePress() {
  return {
      {JointId::THUMB_PIP, 60.0f, 100, 100},
      {JointId::THUMB_MCP, 0.0f, 100, 100},
      {JointId::THUMB_SWING, 20.0f, 100, 100},
      {JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {JointId::FF_PIP, 75.0f, 100, 100},
      {JointId::FF_MCP, 70.0f, 100, 100},
      {JointId::FF_SWING, 0.0f, 100, 100},
      {JointId::MF_PIP, 0.0f, 100, 100},
      {JointId::MF_MCP, 0.0f, 100, 100},
      {JointId::RF_PIP, 75.0f, 100, 100},
      {JointId::RF_MCP, 70.0f, 100, 100},
      {JointId::LF_PIP, 74.0f, 100, 100},
      {JointId::LF_MCP, 70.0f, 100, 100},
  };
}

// Ring finger press pose
std::vector<JointCommand> MakeRingPress() {
  return {
      {JointId::THUMB_PIP, 66.0f, 100, 100},
      {JointId::THUMB_MCP, 0.0f, 100, 100},
      {JointId::THUMB_SWING, 20.0f, 100, 100},
      {JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {JointId::FF_PIP, 75.0f, 100, 100},
      {JointId::FF_MCP, 70.0f, 100, 100},
      {JointId::FF_SWING, 0.0f, 100, 100},
      {JointId::MF_PIP, 75.0f, 100, 100},
      {JointId::MF_MCP, 70.0f, 100, 100},
      {JointId::RF_PIP, 0.0f, 100, 100},
      {JointId::RF_MCP, 0.0f, 100, 100},
      {JointId::LF_PIP, 74.0f, 100, 100},
      {JointId::LF_MCP, 70.0f, 100, 100},
  };
}

// Little finger press pose
std::vector<JointCommand> MakeLittlePress() {
  return {
      {JointId::THUMB_PIP, 66.0f, 100, 100},
      {JointId::THUMB_MCP, 0.0f, 100, 100},
      {JointId::THUMB_SWING, 20.0f, 100, 100},
      {JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {JointId::FF_PIP, 75.0f, 100, 100},
      {JointId::FF_MCP, 70.0f, 100, 100},
      {JointId::FF_SWING, 0.0f, 100, 100},
      {JointId::MF_PIP, 75.0f, 100, 100},
      {JointId::MF_MCP, 70.0f, 100, 100},
      {JointId::RF_PIP, 75.0f, 100, 100},
      {JointId::RF_MCP, 70.0f, 100, 100},
      {JointId::LF_PIP, 0.0f, 100, 100},
      {JointId::LF_MCP, 0.0f, 100, 100},
  };
}

// Open hand pose
std::vector<JointCommand> MakeOpenHand() {
  return {
      {JointId::THUMB_PIP, 0.0f, 100, 100},
      {JointId::THUMB_MCP, 0.0f, 100, 100},
      {JointId::THUMB_SWING, 20.0f, 100, 100},
      {JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {JointId::FF_PIP, 0.0f, 100, 100},
      {JointId::FF_MCP, 0.0f, 100, 100},
      {JointId::FF_SWING, 0.0f, 100, 100},
      {JointId::MF_PIP, 0.0f, 100, 100},
      {JointId::MF_MCP, 0.0f, 100, 100},
      {JointId::RF_PIP, 0.0f, 100, 100},
      {JointId::RF_MCP, 0.0f, 100, 100},
      {JointId::LF_PIP, 0.0f, 100, 100},
      {JointId::LF_MCP, 0.0f, 100, 100},
  };
}

bool Press(DexHand& hand) {
  const std::vector<std::vector<JointCommand>> poses = {
      MakeThumbPress(), MakeIndexPress(), MakeMiddlePress(), MakeRingPress(),
      MakeLittlePress()};
  for (const auto& pose : poses) {
    if (!hand.MoveJoints(pose)) return false;
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  return true;
}

bool HandZero(DexHand& hand) {
  auto joints = MakeOpenHand();
  return hand.MoveJoints(joints);
}

int main() {
  std::cout << "***** 枭尧灵巧手 SDK - 按压功能演示 *****\n" << std::endl;
  auto hand = DexHand::Create(ProductType::G5, CommType::ETHERCAT);
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
  std::cout << "\n--- Device ready, starting press demo ---\n" << std::endl;

  int gesture_cycle = 0;
  const int max_cycles = 0;

  while (true) {
    gesture_cycle++;
    if (max_cycles > 0 && gesture_cycle > max_cycles) break;

    std::cout << "\n--- Round " << gesture_cycle << " demo started ---"
              << std::endl;

    if (!Press(*hand)) {
      std::cout << "第 " << gesture_cycle << " 轮演示中的按压动作执行失败"
                << std::endl;
      break;
    }

    if (!HandZero(*hand)) {
      std::cout << "第 " << gesture_cycle << " 轮演示中的复位动作执行失败"
                << std::endl;
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));

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