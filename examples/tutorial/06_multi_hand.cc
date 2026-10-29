// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

int main() {
  std::cout << "========================================\n"
            << "  GHand Dexterous Hand SDK - Multi-Hand Control Demo\n"
            << "========================================\n";

  // A short-lived Master is enough to enumerate adapters; it is destroyed
  // before any bus is opened.
  std::map<std::string, std::string> adapters;
  {
    auto discovery = ghand::Master::Create(ghand::ProductType::GHand5,
                                           ghand::CommType::ETHERCAT);
    if (!discovery) {
      std::cerr << "Failed to create GHand\n";
      return 1;
    }
    adapters = discovery->SearchAdapters();
  }
  if (adapters.empty()) {
    std::cerr << "No available adapters found\n";
    return 1;
  }

  std::cout << "\nFound " << adapters.size() << " available adapters:\n";
  for (const auto& adapter : adapters) {
    std::cout << "  - " << adapter.first << ": " << adapter.second << '\n';
  }

  // One Master owns one bus. The same bus can appear under several channel
  // names, so only the first channel of each adapter base is opened.
  std::cout << "\nAttempting to connect devices...\n";
  std::vector<std::unique_ptr<ghand::Master>> masters;
  std::vector<ghand::GHand*> hands;
  std::set<std::string> connected_adapter_bases;
  for (const auto& adapter : adapters) {
    const std::string& name = adapter.first;
    const std::string adapter_base = name.substr(0, name.find(':'));
    if (connected_adapter_bases.count(adapter_base) != 0) {
      std::cout << "Skipping alternate channel: " << name << '\n';
      continue;
    }

    auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                        ghand::CommType::ETHERCAT);
    if (!master) {
      std::cerr << "Failed to create GHand for adapter " << name << '\n';
      continue;
    }

    const auto connected = master->Connect(name);
    if (!connected) {
      std::cerr << "Connection failed: " << name << " (" << connected.message
                << ")\n";
      continue;
    }

    hands.insert(hands.end(), connected.value.begin(), connected.value.end());
    masters.push_back(std::move(master));
    connected_adapter_bases.insert(adapter_base);
    std::cout << "Connected device: " << name << '\n';
  }

  if (hands.empty()) {
    std::cerr << "\nFailed to connect any device\n";
    return 1;
  }

  std::cout << "\nSuccessfully connected " << hands.size()
            << " device(s)\n";
  std::cout << "\nDevice Information:\n";
  for (size_t i = 0; i < hands.size(); ++i) {
    const auto result = hands[i]->GetDeviceInfo();
    if (!result) {
      std::cerr << "Failed to read device info: " << result.message << '\n';
      continue;
    }
    const ghand::DeviceInfo& info = result.value;
    std::cout << "  Hand " << i << ":\n"
              << "    Device Name: " << info.device_name << '\n'
              << "    Software Version: " << info.software_version << '\n'
              << "    Serial Number: " << info.serial_number << '\n';
  }

  std::cout << "\n========== Demo 1: Individual Control ==========\n";
  const std::vector<ghand::JointCommand> test_joints = {
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 45.0f, 100, 100},
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 30.0f, 100, 100},
  };
  for (size_t i = 0; i < hands.size(); ++i) {
    std::cout << "\nControlling hand " << i << "...\n";
    const auto result = hands[i]->MoveJoints(test_joints);
    if (!result) {
      std::cerr << "Command send failed: " << result.message << '\n';
    } else {
      std::cout << "Command sent successfully\n";
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  std::cout << "\nHolding position for 2 seconds...\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "\n========== Demo 2: Reset Every Hand ==========\n";
  const std::vector<ghand::JointCommand> reset_joints = {
      {ghand::JointId::THUMB_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
  };

  std::cout << "\nResetting every hand, one hand at a time...\n";
  bool reset_failed = false;
  for (size_t i = 0; i < hands.size(); ++i) {
    const auto result = hands[i]->MoveJoints(reset_joints);
    if (!result) {
      std::cerr << "Hand " << i << " reset failed: " << result.message << '\n';
      reset_failed = true;
    }
  }
  if (!reset_failed) {
    std::cout << "Reset commands sent to " << hands.size() << " hand(s)\n";
  }
  std::cout << "\nWaiting for movement completion...\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "\n========== Cleanup ==========\n";
  for (size_t i = 0; i < masters.size(); ++i) {
    const auto result = masters[i]->Disconnect();
    if (!result) {
      std::cerr << "Failed to close bus " << i << ": " << result.message
                << '\n';
    } else {
      std::cout << "Closed bus " << i << '\n';
    }
  }

  std::cout << "\nDemo completed. Thank you for using!\n";
  return 0;
}
