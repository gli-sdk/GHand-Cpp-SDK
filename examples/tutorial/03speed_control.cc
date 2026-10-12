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
  auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create GHand" << '\n';
    return -1;
  }

  std::cout << "Connecting to dexterous hand..." << '\n';
  const auto connected = master->Connect("auto");
  if (!connected) {
    std::cerr << "Failed to connect to the dexterous hand: "
              << connected.message << '\n';
    return 1;
  }
  ghand::GHand* hand = connected.value.front();

  std::vector<ghand::JointCommand> joints = {
      {ghand::JointId::THUMB_MCP, 0.0f, 10, 10},
      {ghand::JointId::THUMB_TMC_FE, 0.0f, 10, 10},
      {ghand::JointId::FF_PIP, 0.0f, 10, 10},
      {ghand::JointId::FF_MCP, 0.0f, 10, 10},
      {ghand::JointId::MF_PIP, 0.0f, 10, 10},
      {ghand::JointId::MF_MCP, 0.0f, 10, 10},
      {ghand::JointId::RF_PIP, 0.0f, 10, 10},
      {ghand::JointId::RF_MCP, 0.0f, 10, 10},
      {ghand::JointId::LF_PIP, 0.0f, 10, 10},
      {ghand::JointId::LF_MCP, 0.0f, 10, 10},
  };

  std::cout << "Closing fingers (speed=10)..." << '\n';
  const auto close_result = hand->MoveJoints(joints, ghand::ControlMode::SPEED);
  if (!close_result) {
    std::cerr << "Failed to send speed command: " << close_result.message
              << '\n';
    master->Disconnect();
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  std::cout << "Current joint states:" << '\n';
  const auto joint_data = hand->GetJointsData();
  if (joint_data) {
    PrintJointStates(joint_data.value);
  } else {
    std::cerr << "Failed to read joint data: " << joint_data.message << '\n';
  }

  for (auto& joint : joints) {
    joint.velocity = -10;
  }

  std::cout << "Opening fingers (speed=-10)..." << '\n';
  const auto open_result = hand->MoveJoints(joints, ghand::ControlMode::SPEED);
  if (!open_result) {
    std::cerr << "Failed to send speed command: " << open_result.message
              << '\n';
    master->Disconnect();
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  std::cout << "Current joint states:" << '\n';
  const auto opened_data = hand->GetJointsData();
  if (opened_data) {
    PrintJointStates(opened_data.value);
  } else {
    std::cerr << "Failed to read joint data: " << opened_data.message << '\n';
  }

  hand->Stop();
  master->Disconnect();
  std::cout << "Disconnected." << '\n';
  return 0;
}
