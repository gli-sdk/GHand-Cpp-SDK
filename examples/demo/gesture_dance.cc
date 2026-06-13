#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// Gesture pose definitions.

// Thumb touches little finger
std::vector<ghand::JointCommand> MakeThumbTouchLittle() {
  return {
      {ghand::JointId::THUMB_PIP, 20.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 50.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 60.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, 0.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
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
      {ghand::JointId::THUMB_PIP, 19.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 38.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 45.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, 0.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
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
      {ghand::JointId::THUMB_PIP, 17.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 27.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 30.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, 0.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
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
      {ghand::JointId::THUMB_PIP, 13.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 14.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 51.0f, 100, 100},
      {ghand::JointId::FF_MCP, 46.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
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
      {ghand::JointId::THUMB_PIP, 40.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 30.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 30.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 65.0f, 100, 100},
      {ghand::JointId::FF_MCP, 55.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
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
      {ghand::JointId::LF_PIP, 70.0f, 100, 100},
      {ghand::JointId::LF_MCP, 70.0f, 100, 100},
  };
}

// Open middle finger
std::vector<ghand::JointCommand> MakeOpenMiddleFinger() {
  return {
      {ghand::JointId::THUMB_PIP, 30.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 20.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, 0.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
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

// OK pose
std::vector<ghand::JointCommand> MakeOK() {
  return {
      {ghand::JointId::THUMB_PIP, 40.0f, 100, 100},
      {ghand::JointId::THUMB_MCP, 30.0f, 100, 100},
      {ghand::JointId::THUMB_SWING, 30.0f, 100, 100},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 30.0f, 100, 100},
      {ghand::JointId::FF_MCP, 50.0f, 100, 100},
      {ghand::JointId::FF_SWING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, 0.0f, 100, 100},
  };
}

// Action execution functions.

bool HandZero(ghand::DexHand& hand) {
  auto joints = MakeOpenHand();
  return hand.MoveJoints(joints);
}

bool ThumbTouch(ghand::DexHand& hand) {
  const std::vector<std::vector<ghand::JointCommand>> poses = {
      MakeThumbTouchLittle(), MakeThumbTouchRing(), MakeThumbTouchMiddle(),
      MakeThumbTouchIndex()};
  for (const auto& pose : poses) {
    if (!hand.MoveJoints(pose)) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return HandZero(hand);
}

bool FistThenOpen(ghand::DexHand& hand) {
  if (!hand.MoveJoints(MakeFist())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return HandZero(hand);
}

bool SeqOpenFinger(ghand::DexHand& hand) {
  if (!hand.MoveJoints(MakeFist())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (!hand.MoveJoints(MakeOpenIndexFinger())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (!hand.MoveJoints(MakeOpenMiddleFinger())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return HandZero(hand);
}

bool MakeOKGesture(ghand::DexHand& hand) {
  if (!hand.MoveJoints(MakeOK())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return HandZero(hand);
}

// First action group.
bool FirstAction(ghand::DexHand& hand) {
  if (!ThumbTouch(hand)) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  for (int i = 0; i < 2; i++) {
    if (!FistThenOpen(hand)) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return true;
}

// Second action group.
bool SecondAction(ghand::DexHand& hand) {
  if (!SeqOpenFinger(hand)) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (!MakeOKGesture(hand)) return false;
  return true;
}

// Main function.
int main() {
  std::cout << "***** Xiaoyao Dexterous Hand SDK - Gesture Dance Demo *****\n"
            << '\n';
  auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                      ghand::CommType::CANFD);
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
  std::cout << "\n--- Device ready, starting gesture dance demo ---\n"
            << '\n';

  int gesture_cycle = 0;
  const int max_cycles = 0;

  while (true) {
    gesture_cycle++;
    if (max_cycles > 0 && gesture_cycle > max_cycles) break;

    std::cout << "\n--- Round " << gesture_cycle << " gesture demo started ---"
              << '\n';

    if (!FirstAction(*hand)) {
      std::cout << "Round " << gesture_cycle
                << " first action group execution failed" << '\n';
      break;
    }

    if (!SecondAction(*hand)) {
      std::cout << "Round " << gesture_cycle
                << " second action group execution failed" << '\n';
      break;
    }

    std::cout << "--- Round " << gesture_cycle
              << " gesture demo finished ---\n" << '\n';

    if (max_cycles == 0) {
      std::cout << "Press Ctrl+C to stop demo and exit\n" << '\n';
    }
  }

  hand->Disconnect();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "\n--- Demo finished, disconnecting ---" << '\n';
  return 0;
}
