// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ghand/ghand.h"
#include "../example_options.h"

namespace {

std::vector<ghand::JointCommand> BuildPose(float mcp, float pip) {
  return {
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION,
       ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING,
       mcp, 80, 80},
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION,
       ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING,
       pip, 80, 80},
      {ghand::JointId::MF_MCP, ghand::JointControlMode::POSITION,
       ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING,
       mcp, 80, 80},
      {ghand::JointId::MF_PIP, ghand::JointControlMode::POSITION,
       ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING,
       pip, 80, 80},
  };
}

}  // namespace

int main(int argc, char* argv[]) {
  example::Options options{
      ghand::ProductType::GHand5,
      ghand::CommType::ETHERCAT,
      "auto",
  };
  const auto defaults = options;
  const auto parsed = example::ParseOptions(argc, argv, options, false, true);
  if (parsed != example::ParseResult::Run)
    return parsed == example::ParseResult::Help ? 0 : 1;

  auto master = ghand::Master::Create(options.product, options.comm);
  if (!master) {
    std::cerr << "Failed to create GHand\n";
    return 1;
  }

  // This single call covers every transport and topology listed above: "auto"
  // probes the adapters until one carries a controllable hand, and a named
  // device is used directly. Nothing here knows or cares which transport is
  // in use, or whether a junction is present.
  const auto connected = master->Connect(options.device);
  if (!connected) {
    std::cerr << "Connect failed on " << options.device << ": " << connected.message
              << "\n\n";
    example::PrintUsage(argv[0], defaults, false, true);
    return 1;
  }
  const std::vector<ghand::GHand*>& hands = connected.value;

  std::cout << "Connected on " << options.device << " - " << hands.size()
            << " hand(s):\n";
  for (ghand::GHand* hand : hands) {
    const auto result = hand->GetDeviceInfo();
    if (!result) {
      std::cerr << "Failed to read device info: " << result.message << '\n';
      continue;
    }
    const ghand::DeviceInfo& info = result.value;
    std::cout << "  slave 0x" << std::hex << hand->GetSlaveIndex() << std::dec
              << ": " << ghand::ToString(hand->GetHandType())
              << " device=" << info.device_name
              << " serial=" << info.serial_number << '\n';
  }

  // Each callback is registered on one hand and only receives that hand's
  // data, so the latest joints are kept per hand instead of in one shared
  // variable.
  std::mutex state_mutex;
  std::map<uint16_t, std::vector<ghand::JointData>> joints_by_hand;
  for (ghand::GHand* hand : hands) {
    hand->SetJointsCallback([&state_mutex, &joints_by_hand, hand](
                                const std::vector<ghand::JointData>& joints) {
      std::lock_guard<std::mutex> lock(state_mutex);
      joints_by_hand[hand->GetSlaveIndex()] = joints;
    });
  }

  auto print_feedback = [&]() {
    std::lock_guard<std::mutex> lock(state_mutex);
    for (ghand::GHand* hand : hands) {
      const auto it = joints_by_hand.find(hand->GetSlaveIndex());
      if (it == joints_by_hand.end() || it->second.empty()) {
        std::cout << "  slave " << hand->GetSlaveIndex()
                  << ": no feedback yet\n";
        continue;
      }
      const ghand::JointData& first = it->second.front();
      std::cout << "  slave " << hand->GetSlaveIndex() << ": "
                << it->second.size() << " joints, " << ghand::ToString(first.id)
                << " = " << first.angle << " deg\n";
    }
  };

  bool failed = false;

  // Send the shared pose to each hand.
  std::cout << "\nMove the hands to a light grasp pose...\n";
  for (auto* hand : hands) {
    const auto grasp_result = hand->MoveJoints(BuildPose(25.0f, 35.0f));
    if (!grasp_result) {
      std::cerr << "Grasp command failed: " << grasp_result.message << '\n';
      failed = true;
    }
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  print_feedback();

  // A pose that differs per hand is sent hand by hand.
  std::cout << "Mirror the index/middle finger pose...\n";
  for (ghand::GHand* hand : hands) {
    const bool is_left = hand->GetHandType() == ghand::HandType::LEFT_HAND;
    const auto result = hand->MoveJoints(is_left ? BuildPose(45.0f, 60.0f)
                                                 : BuildPose(10.0f, 15.0f));
    if (!result) {
      std::cerr << "Mirror command failed on slave 0x" << std::hex
                << hand->GetSlaveIndex() << std::dec << ": " << result.message
                << '\n';
      failed = true;
    }
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  print_feedback();

  std::cout << "Reset the hands...\n";
  for (auto* hand : hands) {
    const auto reset_result = hand->MoveJoints(BuildPose(0.0f, 0.0f));
    if (!reset_result) {
      std::cerr << "Reset command failed: " << reset_result.message << '\n';
      failed = true;
    }
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  print_feedback();

  for (auto* hand : hands) {
    const auto stop_result = hand->Stop();
    if (!stop_result) {
      std::cerr << "Stop failed: " << stop_result.message << '\n';
      failed = true;
    }
  }
  const auto disconnected = master->Disconnect();
  if (!disconnected) {
    std::cerr << "Disconnect failed: " << disconnected.message << '\n';
    return 1;
  }

  std::cout << "\nDemo completed.\n";
  return failed ? 1 : 0;
}
