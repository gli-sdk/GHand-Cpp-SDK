#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

/**
 * @brief Torque control demonstration
 *
 * This example shows how to control dexterous hand joint movements using
 * different torque percentages. Torque parameter range: 0-100%, higher
 * values produce higher output torque.
 *
 * Note:
 * - Torque control affects the gripping force of fingers
 * - Lower torque is suitable for gentle operations
 * - Higher torque is suitable for operations requiring more force
 */
int main() {
  std::cout << "========================================" << std::endl;
  std::cout << "  GHand Dexterous Hand SDK - Torque Control Demo        "
            << std::endl;
  std::cout << "========================================" << std::endl;

  auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                      ghand::CommType::ETHERCAT);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << std::endl;
    return -1;
  }

  // Connect device
  std::cout << "\nConnecting to dexterous hand via EtherCAT..." << std::endl;
  bool success = hand->AutoConnect();

  if (!success) {
    std::cerr << "Error: Unable to connect to dexterous hand!" << std::endl;
    return 1;
  }

  std::cout << "✓ Successfully connected to dexterous hand!" << std::endl;

  // Set control mode to position mode
  hand->SetControlMode(ghand::ControlMode::POSITION);

  // ========== Demo 1: Fist movement with different torque levels ==========
  std::cout << "\n========== Demo 1: Fist movement with different torque "
               "levels =========="
            << std::endl;
  std::cout << "Demonstrating fist movements at 20%, 40%, 60%, 80%, 100% torque"
            << std::endl;
  std::cout << "Observe the force differences at different torque levels\n"
            << std::endl;

  // Define fist gesture
  std::vector<ghand::JointCommand> fist_joints = {
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

  // Define open hand gesture
  std::vector<ghand::JointCommand> open_joints = {
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

  // Test different torque values
  std::vector<uint8_t> torque_levels = {20, 40, 60, 80, 100};

  for (uint8_t torque : torque_levels) {
    std::cout << ">>> Executing fist, torque: " << static_cast<int>(torque)
              << "%" << std::endl;

    // Set torque and execute fist
    for (auto& joint : fist_joints) {
      joint.torque = torque;
    }
    hand->MoveJoints(fist_joints);

    // Wait for movement completion
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    std::cout << ">>> Opening hand, torque: " << static_cast<int>(torque) << "%"
              << std::endl;

    // Set torque and open hand
    for (auto& joint : open_joints) {
      joint.torque = torque;
    }
    hand->MoveJoints(open_joints);

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    std::cout << std::endl;
  }

  // ========== Demo 2: Torque control for OK gesture ==========
  std::cout << "\n========== Demo 2: Torque control for OK gesture =========="
            << std::endl;
  std::cout << "Executing OK gesture with different torque to simulate gentle "
               "and firm pinching\n"
            << std::endl;

  // Define OK gesture
  std::vector<ghand::JointCommand> ok_joints = {
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

  // Gentle pinch
  std::cout << ">>> Gentle pinch (30% torque)" << std::endl;
  for (auto& joint : ok_joints) {
    joint.torque = 30;
  }
  hand->MoveJoints(ok_joints);
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));

  // Open hand
  hand->MoveJoints(open_joints);
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  // Firm pinch
  std::cout << ">>> Firm pinch (80% torque)" << std::endl;
  for (auto& joint : ok_joints) {
    joint.torque = 80;
  }
  hand->MoveJoints(ok_joints);
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));

  // Open hand
  hand->MoveJoints(open_joints);
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  // ========== Demo 3: Different torque for different fingers ==========
  std::cout << "\n========== Demo 3: Different torque for different fingers "
               "=========="
            << std::endl;
  std::cout << "Setting different torque for each finger to simulate real "
               "grasping scenarios\n"
            << std::endl;

  // Simulate grasping object: thumb and index finger with high torque, other
  // fingers relaxed
  std::vector<ghand::JointCommand> pinch_grip = {
      // Thumb - high torque (mainly for grasping)
      {ghand::JointId::THUMB_PIP, 40.0f, 50, 90},
      {ghand::JointId::THUMB_MCP, 30.0f, 50, 90},
      {ghand::JointId::THUMB_SWING, 30.0f, 50, 90},
      {ghand::JointId::THUMB_ROTATION, 4.0f, 50, 90},
      // Index finger - high torque (mainly for grasping)
      {ghand::JointId::FF_PIP, 65.0f, 50, 90},
      {ghand::JointId::FF_MCP, 55.0f, 50, 90},
      {ghand::JointId::FF_SWING, 0.0f, 50, 90},
      // Middle finger - medium torque (auxiliary support)
      {ghand::JointId::MF_PIP, 65.0f, 50, 50},
      {ghand::JointId::MF_MCP, 55.0f, 50, 50},
      // Ring and little fingers - low torque (relaxed)
      {ghand::JointId::RF_PIP, 65.0f, 50, 30},
      {ghand::JointId::RF_MCP, 55.0f, 50, 30},
      {ghand::JointId::LF_PIP, 65.0f, 50, 30},
      {ghand::JointId::LF_MCP, 55.0f, 50, 30}};

  std::cout << ">>> Executing two-finger pinch (thumb and index finger at 90% "
               "torque, other "
               "fingers at 30-50% torque)"
            << std::endl;
  hand->MoveJoints(pinch_grip);
  std::this_thread::sleep_for(std::chrono::milliseconds(2500));

  std::cout << ">>> Opening hand" << std::endl;
  hand->MoveJoints(open_joints);
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));

  // ========== Demo 4: Gradual torque control ==========
  std::cout << "\n========== Demo 4: Gradual torque control =========="
            << std::endl;
  std::cout << "Gradually increasing from 10% to 100%, observe torque changes\n"
            << std::endl;

  for (int i = 1; i <= 10; i++) {
    uint8_t torque = i * 10;
    std::cout << ">>> Torque: " << static_cast<int>(torque) << "%" << std::endl;

    // Set torque
    for (auto& joint : fist_joints) {
      joint.torque = torque;
    }
    hand->MoveJoints(fist_joints);

    // Wait
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    // Open hand
    for (auto& joint : open_joints) {
      joint.torque = torque;
    }
    hand->MoveJoints(open_joints);

    std::this_thread::sleep_for(std::chrono::milliseconds(800));
  }

  // ========== Demo completed ==========
  std::cout << "\n========== Torque control demo completed =========="
            << std::endl;
  std::cout << "Key points:" << std::endl;
  std::cout << "1. Torque parameter range: 0-100%" << std::endl;
  std::cout << "2. Higher torque values produce higher output torque and "
               "stronger grip"
            << std::endl;
  std::cout << "3. Low torque is suitable for gentle operations and handling "
               "fragile items"
            << std::endl;
  std::cout << "4. High torque is suitable for grasping operations requiring "
               "more force"
            << std::endl;
  std::cout << "5. Each joint can be set with individual torque" << std::endl;
  std::cout
      << "6. Different torque combinations can simulate real grasping scenarios"
      << std::endl;

  // Disconnect
  std::cout << "\nDisconnecting..." << std::endl;
  hand->Disconnect();
  std::cout << "✓ Disconnected" << std::endl;

  std::cout << "\nDemo completed. Thank you for using!" << std::endl;
  return 0;
}
