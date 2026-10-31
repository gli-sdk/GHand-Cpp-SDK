// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only
//
// Subscribes to hand-state and joint feedback for every hand found by
// Connect(). Callbacks run on the communication thread, so this demo only
// prints from them and shares no mutable state with main().

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"
#include "../example_options.h"

namespace {

const char* ToString(ghand::HandRuntimeState state) {
  switch (state) {
    case ghand::HandRuntimeState::INITIALIZED:
      return "INITIALIZED";
    case ghand::HandRuntimeState::POWER_ON_SELF_TEST:
      return "POWER_ON_SELF_TEST";
    case ghand::HandRuntimeState::IDLE:
      return "IDLE";
    case ghand::HandRuntimeState::MOVING:
      return "MOVING";
    case ghand::HandRuntimeState::FAULT:
      return "FAULT";
    case ghand::HandRuntimeState::LOW_POWER:
      return "LOW_POWER";
    case ghand::HandRuntimeState::FIRMWARE_UPGRADE:
      return "FIRMWARE_UPGRADE";
  }
  return "UNKNOWN_STATE";
}

}  // namespace

int main(int argc, char** argv) {
  example::Options options{
      ghand::ProductType::GHand5,
      ghand::CommType::ETHERCAT,
      "auto",
  };
  const auto parsed = example::ParseOptions(argc, argv, options);
  if (parsed != example::ParseResult::Run)
    return parsed == example::ParseResult::Help ? 0 : 1;

  auto master = ghand::Master::Create(options.product, options.comm);
  if (!master) {
    std::cerr << "Failed to create GHand\n";
    return 1;
  }

  const auto connected = master->Connect(options.device);
  if (!connected) {
    std::cerr << "Failed to connect: " << connected.message << '\n';
    return 1;
  }

  for (ghand::GHand* hand : connected.value) {
    const uint16_t index = hand->GetSlaveIndex();
    hand->SetHandStateCallback([index](const ghand::HandData& state) {
      std::cout << "Slave " << index << ": "
                << ghand::ToString(state.joints_state) << '\n';
    });
    hand->SetJointsCallback(
        [index](const std::vector<ghand::JointData>& joints) {
          for (const auto& joint : joints) {
            std::cout << "Slave " << index << ": " << ghand::ToString(joint.id)
                      << ": state=" << ghand::ToString(joint.state)
                      << ", error=" << ghand::ToString(joint.error)
                      << ", angle=" << joint.angle << '\n';
          }
        });

    const auto state = hand->GetHandStateMachine();
    if (state) {
      std::cout << "Slave " << index
                << " state machine: " << ToString(state.value) << '\n';
    } else {
      std::cerr << "Slave " << index << ": " << state.message << '\n';
    }
  }

  std::this_thread::sleep_for(std::chrono::seconds(2));

  const auto disconnected = master->Disconnect();
  if (!disconnected) {
    std::cerr << "Disconnect failed: " << disconnected.message << '\n';
    return 1;
  }
  return 0;
}
