#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

void PrintHeader() {
  std::cout << "========================================" << '\n';
  std::cout << "  GHand Dexterous Hand SDK - Speed Control Demo        "
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
      {ghand::JointId::THUMB_PIP, 40.0f, 0, 50},
      {ghand::JointId::THUMB_MCP, 30.0f, 0, 50},
      {ghand::JointId::THUMB_SWING, 30.0f, 0, 50},
      {ghand::JointId::THUMB_ROTATION, 4.0f, 0, 50},
      {ghand::JointId::FF_PIP, 65.0f, 0, 50},
      {ghand::JointId::FF_MCP, 55.0f, 0, 50},
      {ghand::JointId::MF_PIP, 65.0f, 0, 50},
      {ghand::JointId::MF_MCP, 55.0f, 0, 50},
      {ghand::JointId::RF_PIP, 65.0f, 0, 50},
      {ghand::JointId::RF_MCP, 55.0f, 0, 50},
      {ghand::JointId::LF_PIP, 65.0f, 0, 50},
      {ghand::JointId::LF_MCP, 55.0f, 0, 50}};
}

std::vector<ghand::JointCommand> MakeOpenJoints() {
  return {
      {ghand::JointId::THUMB_PIP, 0.0f, 0, 50},
      {ghand::JointId::THUMB_MCP, 0.0f, 0, 50},
      {ghand::JointId::THUMB_SWING, 0.0f, 0, 50},
      {ghand::JointId::THUMB_ROTATION, 0.0f, 0, 50},
      {ghand::JointId::FF_PIP, 0.0f, 0, 50},
      {ghand::JointId::FF_MCP, 0.0f, 0, 50},
      {ghand::JointId::MF_PIP, 0.0f, 0, 50},
      {ghand::JointId::MF_MCP, 0.0f, 0, 50},
      {ghand::JointId::RF_PIP, 0.0f, 0, 50},
      {ghand::JointId::RF_MCP, 0.0f, 0, 50},
      {ghand::JointId::LF_PIP, 0.0f, 0, 50},
      {ghand::JointId::LF_MCP, 0.0f, 0, 50}};
}

void SetSpeed(std::vector<ghand::JointCommand>* joints, uint8_t speed) {
  for (auto& joint : *joints) {
    joint.velocity = speed;
  }
}

void RunSpeedLevelDemo(ghand::DexHand& hand) {
  std::cout
      << "\n========== Demo 1: Fist movement with different speeds =========="
      << '\n';
  std::cout << "Demonstrating fist movements at 25%, 50%, 75%, 100% speeds"
            << '\n';
  std::cout << "Observe the speed differences of finger movements at "
               "different speeds\n"
            << '\n';

  std::vector<ghand::JointCommand> fist_joints = MakeFistJoints();
  std::vector<ghand::JointCommand> open_joints = MakeOpenJoints();
  std::vector<uint8_t> speed_levels = {25, 50, 75, 100};
  for (uint8_t speed : speed_levels) {
    std::cout << ">>> Executing fist, speed: " << static_cast<int>(speed)
              << "%" << '\n';
    SetSpeed(&fist_joints, speed);
    hand.MoveJoints(fist_joints);
    int wait_time = 1500 + (100 - speed) * 10;
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));

    std::cout << ">>> Opening hand, speed: " << static_cast<int>(speed)
              << "%" << '\n';
    SetSpeed(&open_joints, speed);
    hand.MoveJoints(open_joints);
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
    std::cout << '\n';
  }
}

void RunGradualSpeedDemo(ghand::DexHand& hand) {
  std::cout << "\n========== Demo 2: Gradual speed control =========="
            << '\n';
  std::cout << "Gradually increasing from 10% to 100%, observe speed changes\n"
            << '\n';

  std::vector<ghand::JointCommand> fist_joints = MakeFistJoints();
  std::vector<ghand::JointCommand> open_joints = MakeOpenJoints();
  for (int i = 1; i <= 10; ++i) {
    uint8_t speed = static_cast<uint8_t>(i * 10);
    std::cout << ">>> Speed: " << static_cast<int>(speed) << "%" << '\n';
    SetSpeed(&fist_joints, speed);
    hand.MoveJoints(fist_joints);
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    SetSpeed(&open_joints, speed);
    hand.MoveJoints(open_joints);
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
  }
}

std::vector<ghand::JointCommand> MakeWaveFist() {
  return {
      {ghand::JointId::THUMB_PIP, 40.0f, 100, 50},
      {ghand::JointId::THUMB_MCP, 30.0f, 100, 50},
      {ghand::JointId::THUMB_SWING, 30.0f, 100, 50},
      {ghand::JointId::THUMB_ROTATION, 4.0f, 100, 50},
      {ghand::JointId::FF_PIP, 65.0f, 80, 50},
      {ghand::JointId::FF_MCP, 55.0f, 80, 50},
      {ghand::JointId::MF_PIP, 65.0f, 60, 50},
      {ghand::JointId::MF_MCP, 55.0f, 60, 50},
      {ghand::JointId::RF_PIP, 65.0f, 40, 50},
      {ghand::JointId::RF_MCP, 55.0f, 40, 50},
      {ghand::JointId::LF_PIP, 65.0f, 20, 50},
      {ghand::JointId::LF_MCP, 55.0f, 20, 50}};
}

void RunWaveSpeedDemo(ghand::DexHand& hand) {
  std::cout << "\n========== Demo 3: Different speeds for different fingers "
               "=========="
            << '\n';
  std::cout
      << "Setting different speeds for each finger to create wave effect\n"
      << '\n';

  std::cout << ">>> Executing wave fist (thumb fastest, little finger slowest)"
            << '\n';
  hand.MoveJoints(MakeWaveFist());
  std::this_thread::sleep_for(std::chrono::milliseconds(2500));

  std::cout << ">>> Opening hand" << '\n';
  hand.MoveJoints(MakeOpenJoints());
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

void PrintSummary() {
  std::cout << "\n========== Speed control demo completed =========="
            << '\n';
  std::cout << "Key points:" << '\n';
  std::cout << "1. Speed parameter range: 0-100%" << '\n';
  std::cout << "2. Higher speed values result in faster joint movement"
            << '\n';
  std::cout << "3. Each joint can be set with individual speed" << '\n';
  std::cout
      << "4. Different speed combinations can create complex motion effects"
      << '\n';
}

int main() {
  PrintHeader();
  auto hand = CreateConnectedHand();
  if (!hand) return 1;

  RunSpeedLevelDemo(*hand);
  RunGradualSpeedDemo(*hand);
  RunWaveSpeedDemo(*hand);
  PrintSummary();

  std::cout << "\nDisconnecting..." << '\n';
  hand->Disconnect();
  std::cout << "OK Disconnected" << '\n';
  std::cout << "\nDemo completed. Thank you for using!" << '\n';
  return 0;
}
