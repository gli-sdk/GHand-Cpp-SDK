#include <chrono>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

int main() {
  std::cout << "========================================\n";
  std::cout << "  GHand Dexterous Hand SDK - Multi-Hand Control Demo\n";
  std::cout << "========================================\n";

  // Search for network adapters
  auto temp_hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                           ghand::CommType::ETHERCAT);
  if (!temp_hand) {
    std::cerr << "Failed to create DexHand" << std::endl;
    return -1;
  }
  std::map<std::string, std::string> adapters = temp_hand->SearchAdapters();

  if (adapters.empty()) {
    std::cerr << "No available network adapters found\n";
    return 1;
  }

  std::cout << "\nFound " << adapters.size() << " available adapters:\n";
  for (const auto& adapter : adapters) {
    const std::string& name = adapter.first;
    const std::string& desc = adapter.second;
    std::cout << "  - " << name << ": " << desc << '\n';
  }

  // Create multiple hand instances
  std::vector<std::unique_ptr<ghand::DexHand>> hands;

  std::cout << "\nAttempting to connect devices...\n";
  for (const auto& adapter : adapters) {
    const std::string& name = adapter.first;
    auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                        ghand::CommType::ETHERCAT);
    if (hand->Connect(name)) {
      hands.push_back(std::move(hand));
      std::cout << "Connected device: " << name << '\n';
    } else {
      std::cout << "Connection failed: " << name << '\n';
    }
  }

  if (hands.empty()) {
    std::cerr << "\nFailed to connect any device\n";
    return 1;
  }

  std::cout << "\nSuccessfully connected " << hands.size() << " device(s)\n";

  // Display all device information
  std::cout << "\nDevice Information:\n";
  for (size_t i = 0; i < hands.size(); ++i) {
    ghand::DeviceInfo info = hands[i]->GetDeviceInfo();
    std::cout << "  Hand " << i << ":\n";
    std::cout << "    Device Name: " << info.device_name << '\n';
    std::cout << "    Hardware Version: " << info.hardware_version << '\n';
    std::cout << "    Software Version: " << info.software_version << '\n';
    std::cout << "    Serial Number: " << info.serial_number << '\n';
  }

  // Demo 1: Control each hand individually
  std::cout << "\n========== Demo 1: Individual Control ==========\n";

  std::vector<ghand::JointCommand> test_joints = {
      {ghand::JointId::FF_PIP, 45.0f, 100, 100},
      {ghand::JointId::FF_MCP, 30.0f, 100, 100},
  };

  for (size_t i = 0; i < hands.size(); ++i) {
    std::cout << "\nControlling hand " << i << "...\n";

    if (hands[i]->MoveJoints(test_joints)) {
      std::cout << "Command sent successfully\n";
    } else {
      std::cout << "Command send failed\n";
    }

    // Wait for device to respond
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // Hold position
  std::cout << "\nHolding position for 2 seconds...\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));

  // Demo 2: Control all hands simultaneously
  std::cout << "\n========== Demo 2: Simultaneous Control ==========\n";

  std::vector<ghand::JointCommand> reset_joints = {
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

  std::cout << "\nResetting all hands simultaneously...\n";
  for (auto& hand : hands) {
    hand->MoveJoints(reset_joints);
  }
  std::cout << "Commands sent to all hands\n";

  // Wait for movement completion
  std::cout << "\nWaiting for movement completion...\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));

  // Disconnect all
  std::cout << "\n========== Cleanup ==========\n";
  std::cout << "\nDisconnecting all connections...\n";
  for (size_t i = 0; i < hands.size(); ++i) {
    hands[i]->Disconnect();
    std::cout << "Closed hand " << i << '\n';
  }

  std::cout << "\nDemo completed. Thank you for using!\n";
  return 0;
}
