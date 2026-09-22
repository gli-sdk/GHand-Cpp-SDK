#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// Joint status display callback function. It is registered on one hand, so it
// only ever receives that hand's data.
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
  auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create GHand" << '\n';
    return -1;
  }

  // Try to connect to the dexterous hand
  std::cout << "Connecting to dexterous hand..." << '\n';
  const auto connected = master->Connect("auto");
  if (!connected) {
    std::cout << "Failed to connect: " << connected.message << '\n';
    return 1;
  }

  std::cout << "Successfully connected to the dexterous hand!" << '\n';
  ghand::GHand* hand = connected.value.front();

  // Register joint status callback to display joint data in real time
  hand->SetJointsCallback(DisplayJoints);
  std::cout << "Joint display callback registered." << '\n';

  // Define joint commands: control all 13 joints
  // DIP joints will be automatically skipped, no need to control
  std::vector<ghand::JointCommand> joints = {
      {ghand::JointId::FF_MCP, 30.0f, 100, 100},
      {ghand::JointId::FF_PIP, 45.0f, 100, 100},
  };

  std::cout << "Moving joints..." << '\n';
  const auto move_result = hand->MoveJoints(joints);
  if (move_result) {
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
    std::cout << "Failed to move joints: " << move_result.message << '\n';
  }

  master->Disconnect();
  std::cout << "Connection closed." << '\n';

  return 0;
}
