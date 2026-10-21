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

namespace {

const char* ToString(ghand::HandStateMachine state) {
  switch (state) {
    case ghand::HandStateMachine::INITIALIZED:
      return "INITIALIZED";
    case ghand::HandStateMachine::POWER_ON_SELF_TEST:
      return "POWER_ON_SELF_TEST";
    case ghand::HandStateMachine::IDLE:
      return "IDLE";
    case ghand::HandStateMachine::MOVING:
      return "MOVING";
    case ghand::HandStateMachine::FAULT:
      return "FAULT";
    case ghand::HandStateMachine::LOW_POWER:
      return "LOW_POWER";
    case ghand::HandStateMachine::FIRMWARE_UPGRADE:
      return "FIRMWARE_UPGRADE";
    case ghand::HandStateMachine::UNKNOWN_STATE:
      return "UNKNOWN_STATE";
  }
  return "UNKNOWN_STATE";
}

}  // namespace

int main() {
  auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create GHand\n";
    return 1;
  }

  const auto connected = master->Connect("auto");
  if (!connected) {
    std::cerr << "Failed to connect: " << connected.message << '\n';
    return 1;
  }

  for (ghand::GHand* hand : connected.value) {
    const uint16_t index = hand->slave_index();
    hand->SetHandStateCallback([index](const ghand::HandState& state) {
      std::cout << "Slave " << index << ": " << ghand::ToString(state) << '\n';
    });
    hand->SetJointsCallback(
        [index](const std::vector<ghand::Joint>& joints) {
          for (const auto& joint : joints) {
            std::cout << "Slave " << index << ": " << ghand::ToString(joint)
                      << '\n';
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
