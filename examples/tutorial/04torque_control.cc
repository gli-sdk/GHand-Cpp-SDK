#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

void PrintHeader() {
  std::cout << "========================================" << '\n';
  std::cout << "  GHand Dexterous Hand SDK - Torque Control Demo        "
            << '\n';
  std::cout << "========================================" << '\n';
}

std::unique_ptr<ghand::DexHand> CreateConnectedHand() {
  auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                      ghand::CommType::CANFD);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << '\n';
    return nullptr;
  }

  std::cout << "\nConnecting to dexterous hand via CANFD..." << '\n';
  if (!hand->AutoConnect()) {
    std::cerr << "Error: Unable to connect to dexterous hand!" << '\n';
    return nullptr;
  }

  std::cout << "OK Successfully connected to dexterous hand!" << '\n';
  hand->SetControlMode(ghand::ControlMode::POSITION);
  return hand;
}

std::vector<ghand::JointCommand> MakeFistJoints() {
  return {
      {ghand::JointId::THUMB_PIP, 40.0f, 50, 0},
      {ghand::JointId::THUMB_MCP, 30.0f, 50, 0},
      {ghand::JointId::THUMB_SWING, 30.0f, 50, 0},
      {ghand::JointId::THUMB_ROTATION, 4.0f, 50, 0},
      {ghand::JointId::FF_PIP, 65.0f, 50, 0},
      {ghand::JointId::FF_MCP, 55.0f, 50, 0},
      {ghand::JointId::MF_PIP, 65.0f, 50, 0},
      {ghand::JointId::MF_MCP, 55.0f, 50, 0},
      {ghand::JointId::RF_PIP, 65.0f, 50, 0},
      {ghand::JointId::RF_MCP, 55.0f, 50, 0},
      {ghand::JointId::LF_PIP, 65.0f, 50, 0},
      {ghand::JointId::LF_MCP, 55.0f, 50, 0}};
}

std::vector<ghand::JointCommand> MakeOpenJoints() {
  return {
      {ghand::JointId::THUMB_PIP, 0.0f, 50, 0},
      {ghand::JointId::THUMB_MCP, 0.0f, 50, 0},
      {ghand::JointId::THUMB_SWING, 0.0f, 50, 0},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 50, 0},
      {ghand::JointId::FF_PIP, 0.0f, 50, 0},
      {ghand::JointId::FF_MCP, 0.0f, 50, 0},
      {ghand::JointId::MF_PIP, 0.0f, 50, 0},
      {ghand::JointId::MF_MCP, 0.0f, 50, 0},
      {ghand::JointId::RF_PIP, 0.0f, 50, 0},
      {ghand::JointId::RF_MCP, 0.0f, 50, 0},
      {ghand::JointId::LF_PIP, 0.0f, 50, 0},
      {ghand::JointId::LF_MCP, 0.0f, 50, 0}};
}

void SetTorque(std::vector<ghand::JointCommand>* joints, uint8_t torque) {
  for (auto& joint : *joints) {
    joint.torque = torque;
  }
}

void RunTorqueLevelDemo(ghand::DexHand& hand) {
  std::cout << "\n========== Demo 1: Fist movement with different torque "
               "levels =========="
            << '\n';
  std::cout << "Demonstrating fist movements at 20%, 40%, 60%, "
               "80%, 100% torque"
            << '\n';
  std::cout << "Observe the force differences at different torque levels\n"
            << '\n';

  std::vector<ghand::JointCommand> fist_joints = MakeFistJoints();
  std::vector<ghand::JointCommand> open_joints = MakeOpenJoints();
  std::vector<uint8_t> torque_levels = {20, 40, 60, 80, 100};
  for (uint8_t torque : torque_levels) {
    std::cout << ">>> Executing fist, torque: " << static_cast<int>(torque)
              << "%" << '\n';
    SetTorque(&fist_joints, torque);
    hand.MoveJoints(fist_joints);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    std::cout << ">>> Opening hand, torque: " << static_cast<int>(torque)
              << "%" << '\n';
    SetTorque(&open_joints, torque);
    hand.MoveJoints(open_joints);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    std::cout << '\n';
  }
}

std::vector<ghand::JointCommand> MakeOkJoints() {
  return {
      {ghand::JointId::THUMB_PIP, 40.0f, 50, 0},
      {ghand::JointId::THUMB_MCP, 30.0f, 50, 0},
      {ghand::JointId::THUMB_SWING, 30.0f, 50, 0},
      {ghand::JointId::THUMB_ROTATION, 4.0f, 50, 0},
      {ghand::JointId::FF_PIP, 30.0f, 50, 0},
      {ghand::JointId::FF_MCP, 50.0f, 50, 0},
      {ghand::JointId::FF_SWING, 0.0f, 50, 0},
      {ghand::JointId::MF_PIP, 0.0f, 50, 0},
      {ghand::JointId::MF_MCP, 0.0f, 50, 0},
      {ghand::JointId::RF_PIP, 0.0f, 50, 0},
      {ghand::JointId::RF_MCP, 0.0f, 50, 0},
      {ghand::JointId::LF_PIP, 0.0f, 50, 0},
      {ghand::JointId::LF_MCP, 0.0f, 50, 0}};
}

void RunOkTorqueDemo(ghand::DexHand& hand) {
  std::cout << "\n========== Demo 2: Torque control for OK gesture =========="
            << '\n';
  std::cout << "Executing OK gesture with different torque to simulate gentle "
               "and firm pinching\n"
            << '\n';

  std::vector<ghand::JointCommand> ok_joints = MakeOkJoints();
  std::cout << ">>> Gentle pinch (30% torque)" << '\n';
  SetTorque(&ok_joints, 30);
  hand.MoveJoints(ok_joints);
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));

  hand.MoveJoints(MakeOpenJoints());
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  std::cout << ">>> Firm pinch (80% torque)" << '\n';
  SetTorque(&ok_joints, 80);
  hand.MoveJoints(ok_joints);
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));

  hand.MoveJoints(MakeOpenJoints());
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
}

std::vector<ghand::JointCommand> MakePinchGrip() {
  return {
      {ghand::JointId::THUMB_PIP, 40.0f, 50, 90},
      {ghand::JointId::THUMB_MCP, 30.0f, 50, 90},
      {ghand::JointId::THUMB_SWING, 30.0f, 50, 90},
      {ghand::JointId::THUMB_ROTATION, 4.0f, 50, 90},
      {ghand::JointId::FF_PIP, 65.0f, 50, 90},
      {ghand::JointId::FF_MCP, 55.0f, 50, 90},
      {ghand::JointId::FF_SWING, 0.0f, 50, 90},
      {ghand::JointId::MF_PIP, 65.0f, 50, 50},
      {ghand::JointId::MF_MCP, 55.0f, 50, 50},
      {ghand::JointId::RF_PIP, 65.0f, 50, 30},
      {ghand::JointId::RF_MCP, 55.0f, 50, 30},
      {ghand::JointId::LF_PIP, 65.0f, 50, 30},
      {ghand::JointId::LF_MCP, 55.0f, 50, 30}};
}

void RunFingerTorqueDemo(ghand::DexHand& hand) {
  std::cout << "\n========== Demo 3: Different torque for different fingers "
               "=========="
            << '\n';
  std::cout << "Setting different torque for each finger to simulate real "
               "grasping scenarios\n"
            << '\n';

  std::cout << ">>> Executing two-finger pinch (thumb and index finger at 90% "
               "torque, other fingers at 30-50% torque)"
            << '\n';
  hand.MoveJoints(MakePinchGrip());
  std::this_thread::sleep_for(std::chrono::milliseconds(2500));

  std::cout << ">>> Opening hand" << '\n';
  hand.MoveJoints(MakeOpenJoints());
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

void RunGradualTorqueDemo(ghand::DexHand& hand) {
  std::cout << "\n========== Demo 4: Gradual torque control =========="
            << '\n';
  std::cout << "Gradually increasing from 10% to 100%, observe torque "
               "changes\n"
            << '\n';

  std::vector<ghand::JointCommand> fist_joints = MakeFistJoints();
  std::vector<ghand::JointCommand> open_joints = MakeOpenJoints();
  for (int i = 1; i <= 10; ++i) {
    uint8_t torque = static_cast<uint8_t>(i * 10);
    std::cout << ">>> Torque: " << static_cast<int>(torque) << "%" << '\n';
    SetTorque(&fist_joints, torque);
    hand.MoveJoints(fist_joints);
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    SetTorque(&open_joints, torque);
    hand.MoveJoints(open_joints);
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
  }
}

void PrintSummary() {
  std::cout << "\n========== Torque control demo completed =========="
            << '\n';
  std::cout << "Key points:" << '\n';
  std::cout << "1. Torque parameter range: 0-100%" << '\n';
  std::cout << "2. Higher torque values produce higher output torque and "
               "stronger grip"
            << '\n';
  std::cout << "3. Low torque is suitable for gentle operations and handling "
               "fragile items"
            << '\n';
  std::cout << "4. High torque is suitable for grasping operations requiring "
               "more force"
            << '\n';
  std::cout << "5. Each joint can be set with individual torque" << '\n';
  std::cout << "6. Different torque combinations can simulate real grasping "
               "scenarios"
            << '\n';
}

int main() {
  PrintHeader();
  auto hand = CreateConnectedHand();
  if (!hand) return 1;

  RunTorqueLevelDemo(*hand);
  RunOkTorqueDemo(*hand);
  RunFingerTorqueDemo(*hand);
  RunGradualTorqueDemo(*hand);
  PrintSummary();

  std::cout << "\nDisconnecting..." << '\n';
  hand->Disconnect();
  std::cout << "OK Disconnected" << '\n';
  std::cout << "\nDemo completed. Thank you for using!" << '\n';
  return 0;
}
