#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

#include "ghand/ghand.h"

// Joint status display callback function
void DisplayJoints(const std::vector<ghand::Joint>& joints) {
  std::cout << "\n========== Joint Status ==========" << '\n';
  std::cout << std::left << std::setw(20) << "Joint" << std::setw(10)
            << "Angle(deg)" << std::setw(10) << "Velocity(%)" << std::setw(10)
            << "Torque(%)" << std::setw(15) << "State"
            << "Error" << '\n';
  std::cout << std::string(80, '-') << '\n';

  for (const auto& joint : joints) {
    std::cout << std::left << std::setw(20) << ghand::ToString(joint.id)
              << std::fixed << std::setprecision(1) << std::setw(10)
              << joint.angle << std::setw(15) << +joint.velocity
              << std::setw(15) << +joint.torque << std::setw(15)
              << ghand::ToString(joint.state) << ghand::ToString(joint.error)
              << '\n';
  }
  std::cout << "==================================" << '\n';
}

int main() {
  auto hand =
      ghand::DexHand::Create(ghand::ProductType::G5, ghand::CommType::CANFD);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << '\n';
    return -1;
  }

  // Try to connect to the dexterous hand via CANFD
  std::cout << "Connecting to dexterous hand via CANFD..." << '\n';
  bool success = hand->AutoConnect();

  if (success) {
    std::cout << "Successfully connected to the dexterous hand!" << '\n';

    // Register joint status callback to display joint data in real time
    hand->SetJointsCallback(DisplayJoints);
    std::cout << "Joint display callback registered." << '\n';

    // Set control mode to position mode (default)
    hand->SetControlMode(ghand::ControlMode::POSITION);

    // Define joint commands: control all 13 joints
    // DIP joints will be automatically skipped, no need to control
    std::vector<ghand::JointCommand> joints = {
        {ghand::JointId::FF_MCP, 30.0f, 100, 100},
        {ghand::JointId::FF_PIP, 45.0f, 100, 100},
    };

    std::cout << "Moving joints..." << '\n';
    bool move_success = hand->MoveJoints(joints);

    if (move_success) {
      std::cout << "Joints moved successfully!" << '\n';

      // Hold pose for 5 seconds
      std::this_thread::sleep_for(std::chrono::seconds(5));

      // Reset joint positions
      std::vector<ghand::JointCommand> reset_joints = {
          {ghand::JointId::FF_MCP, 0.0f, 100, 100},
          {ghand::JointId::FF_PIP, 0.0f, 100, 100}};

      std::cout << "Resetting joint positions..." << '\n';
      hand->MoveJoints(reset_joints);
      std::this_thread::sleep_for(std::chrono::seconds(2));
    } else {
      std::cout << "Failed to move joints!" << '\n';
    }

    hand->Disconnect();
    std::cout << "Connection closed." << '\n';
  } else {
    std::cout << "Failed to connect to the dexterous hand!" << '\n';
  }

  return 0;
}
