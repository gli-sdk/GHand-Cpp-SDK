#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

/**
 * @brief Speed control demonstration
 *
 * This example shows how to control dexterous hand joint movements using
 * different speed percentages. Speed parameter range: 0-100%, higher values
 * result in faster movement.
 */
int main() {
  std::cout << "========================================" << '\n';
  std::cout << "  GHand Dexterous Hand SDK - Speed Control Demo        "
            << '\n';
  std::cout << "========================================" << '\n';

  auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                      ghand::CommType::CANFD);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << '\n';
    return -1;
  }

  // Connect device
  std::cout << "\nConnecting to dexterous hand via CANFD..." << '\n';
  bool success = hand->AutoConnect();

  if (!success) {
    std::cerr << "Error: Unable to connect to dexterous hand!" << '\n';
    return 1;
  }

  std::cout << "✓ Successfully connected to dexterous hand!" << '\n';

  // Set control mode to position mode
  hand->SetControlMode(ghand::ControlMode::POSITION);

  // ========== Demo 1: Fist movement with different speeds ==========
  std::cout
      << "\n========== Demo 1: Fist movement with different speeds =========="
      << '\n';
  std::cout << "Demonstrating fist movements at 25%, 50%, 75%, 100% speeds"
            << '\n';
  std::cout << "Observe the speed differences of finger movements at different "
               "speeds\n"
            << '\n';

  // Define fist gesture
  std::vector<ghand::JointCommand> fist_joints = {
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

  // Define open hand gesture
  std::vector<ghand::JointCommand> open_joints = {
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

  // Test different speed values
  std::vector<uint8_t> speed_levels = {25, 50, 75, 100};

  for (uint8_t speed : speed_levels) {
    std::cout << ">>> Executing fist, speed: " << static_cast<int>(speed) << "%"
              << '\n';

    // Set speed and execute fist
    for (auto& joint : fist_joints) {
      joint.velocity = speed;
    }
    hand->MoveJoints(fist_joints);

    // Wait for movement completion (faster speed = shorter wait time)
    int wait_time = 1500 + (100 - speed) * 10;
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));

    std::cout << ">>> Opening hand, speed: " << static_cast<int>(speed) << "%"
              << '\n';

    // Set speed and open hand
    for (auto& joint : open_joints) {
      joint.velocity = speed;
    }
    hand->MoveJoints(open_joints);

    std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
    std::cout << '\n';
  }

  // ========== Demo 2: Gradual speed control ==========
  std::cout << "\n========== Demo 2: Gradual speed control =========="
            << '\n';
  std::cout << "Gradually increasing from 10% to 100%, observe speed changes\n"
            << '\n';

  for (int i = 1; i <= 10; i++) {
    uint8_t speed = i * 10;
    std::cout << ">>> Speed: " << static_cast<int>(speed) << "%" << '\n';

    // Set speed
    for (auto& joint : fist_joints) {
      joint.velocity = speed;
    }
    hand->MoveJoints(fist_joints);

    // Wait
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    // Open hand
    for (auto& joint : open_joints) {
      joint.velocity = speed;
    }
    hand->MoveJoints(open_joints);

    std::this_thread::sleep_for(std::chrono::milliseconds(800));
  }

  // ========== Demo 3: Different speeds for different fingers ==========
  std::cout << "\n========== Demo 3: Different speeds for different fingers "
               "=========="
            << '\n';
  std::cout
      << "Setting different speeds for each finger to create wave effect\n"
      << '\n';

  // Create joint commands with each finger moving at different speeds
  std::vector<ghand::JointCommand> wave_fist = {
      // Thumb - fastest
      {ghand::JointId::THUMB_PIP, 40.0f, 100, 50},
      {ghand::JointId::THUMB_MCP, 30.0f, 100, 50},
      {ghand::JointId::THUMB_SWING, 30.0f, 100, 50},
      {ghand::JointId::THUMB_ROTATION, 4.0f, 100, 50},
      // Index finger - fast
      {ghand::JointId::FF_PIP, 65.0f, 80, 50},
      {ghand::JointId::FF_MCP, 55.0f, 80, 50},
      // Middle finger - medium
      {ghand::JointId::MF_PIP, 65.0f, 60, 50},
      {ghand::JointId::MF_MCP, 55.0f, 60, 50},
      // Ring finger - slow
      {ghand::JointId::RF_PIP, 65.0f, 40, 50},
      {ghand::JointId::RF_MCP, 55.0f, 40, 50},
      // Little finger - slowest
      {ghand::JointId::LF_PIP, 65.0f, 20, 50},
      {ghand::JointId::LF_MCP, 55.0f, 20, 50}};

  std::cout << ">>> Executing wave fist (thumb fastest, little finger slowest)"
            << '\n';
  hand->MoveJoints(wave_fist);
  std::this_thread::sleep_for(std::chrono::milliseconds(2500));

  std::cout << ">>> Opening hand" << '\n';
  hand->MoveJoints(open_joints);
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));

  // ========== Demo completed ==========
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

  // Disconnect
  std::cout << "\nDisconnecting..." << '\n';
  hand->Disconnect();
  std::cout << "✓ Disconnected" << '\n';

  std::cout << "\nDemo completed. Thank you for using!" << '\n';
  return 0;
}
