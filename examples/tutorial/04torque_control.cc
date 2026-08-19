#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

void PrintJointStates(const std::vector<ghand::Joint>& joints) {
  for (const auto& joint : joints) {
    if (joint.id != ghand::JointId::THUMB_MCP &&
        joint.id != ghand::JointId::FF_PIP &&
        joint.id != ghand::JointId::MF_PIP) {
      continue;
    }

    std::cout << "  " << std::left << std::setw(15)
              << ghand::ToString(joint.id)
              << "- state: " << ghand::ToString(joint.state)
              << ", error: " << ghand::ToString(joint.error)
              << ", angle: " << std::fixed << std::setprecision(2)
              << joint.angle << " deg"
              << ", speed: " << static_cast<int>(joint.velocity)
              << ", torque: " << static_cast<int>(joint.torque) << '\n';
  }
}

int main() {
  auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                      ghand::CommType::ETHERCAT);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << '\n';
    return -1;
  }

  std::cout << "Connecting to dexterous hand..." << '\n';
  if (!hand->AutoConnect()) {
    std::cerr << "Failed to connect to the dexterous hand!" << '\n';
    return 1;
  }

  hand->SetControlMode(ghand::ControlMode::TORQUE);

  std::vector<ghand::JointCommand> joints = {
      {ghand::JointId::THUMB_MCP, 0.0f, 0, 10},
      {ghand::JointId::THUMB_TMC_FE, 0.0f, 0, 10},
      {ghand::JointId::FF_PIP, 0.0f, 0, 10},
      {ghand::JointId::FF_MCP, 0.0f, 0, 10},
      {ghand::JointId::MF_PIP, 0.0f, 0, 10},
      {ghand::JointId::MF_MCP, 0.0f, 0, 10},
      {ghand::JointId::RF_PIP, 0.0f, 0, 10},
      {ghand::JointId::RF_MCP, 0.0f, 0, 10},
      {ghand::JointId::LF_PIP, 0.0f, 0, 10},
      {ghand::JointId::LF_MCP, 0.0f, 0, 10},
  };

  std::cout << "Closing fingers (torque=10)..." << '\n';
  if (!hand->MoveJoints(joints)) {
    std::cerr << "Failed to send torque command" << '\n';
    hand->Disconnect();
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  std::cout << "Current joint states:" << '\n';
  PrintJointStates(hand->GetJointsData());

  for (auto& joint : joints) {
    joint.torque = -10;
  }

  std::cout << "Opening fingers (torque=-10)..." << '\n';
  if (!hand->MoveJoints(joints)) {
    std::cerr << "Failed to send torque command" << '\n';
    hand->Disconnect();
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  std::cout << "Current joint states:" << '\n';
  PrintJointStates(hand->GetJointsData());

  hand->Stop();
  hand->Disconnect();
  std::cout << "Disconnected." << '\n';
  return 0;
}
