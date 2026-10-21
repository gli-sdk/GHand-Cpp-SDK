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

bool HandZero(ghand::Master& master) {
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
  return HandZero(master);
}

bool FistThenOpen(ghand::Master& master) {
  if (!master.MoveJoints(MakeFist())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return HandZero(master);
}

bool SeqOpenFinger(ghand::Master& master) {
  if (!master.MoveJoints(MakeFist())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (!master.MoveJoints(MakeOpenIndexFinger())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (!master.MoveJoints(MakeOpenMiddleFinger())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return HandZero(master);
}

bool MakeOKGesture(ghand::Master& master) {
  if (!master.MoveJoints(MakeOK())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return HandZero(master);
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

// Main function.
int main() {
  std::cout << "***** GHand SDK - Gesture Dance Demo *****\n"
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
  std::cout << "\n--- Device ready, starting gesture dance demo ---\n"
            << '\n';

  int gesture_cycle = 0;
  const int max_cycles = 0;

  while (true) {
    gesture_cycle++;
    if (max_cycles > 0 && gesture_cycle > max_cycles) break;

    std::cout << "\n--- Round " << gesture_cycle << " gesture demo started ---"
              << '\n';

    if (!FirstAction(*master)) {
      std::cout << "Round " << gesture_cycle
                << " first action group execution failed" << '\n';
      break;
    }

    if (!SecondAction(*master)) {
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

  master->Disconnect();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "\n--- Demo finished, disconnecting ---" << '\n';
  return 0;
}
