// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only
#include <chrono>
#include <iostream>
#include <thread>
#include "ghand/ghand.h"
int main() {
  auto master = ghand::Master::Create(ghand::ProductType::GHand5, ghand::CommType::ETHERCAT);
  if (!master) return 1;
  auto connected = master->Connect("auto");
  if (!connected) { std::cerr << connected.message << '\n'; return 1; }
  for (auto hand : connected.value) {
    auto index = hand->slave_index();
    hand->SetHandStateCallback([index](const ghand::HandState& state) {
      std::cout << "Slave " << index << ": " << ghand::ToString(state) << '\n';
    });
    hand->SetJointsCallback([index](const std::vector<ghand::Joint>& joints) {
      for (const auto& joint : joints) {
        std::cout << "Slave " << index << ": " << ghand::ToString(joint) << '\n';
      }
    });
    auto state = hand->GetHandStateMachine();
    if (state) std::cout << "Slave " << index << " state machine: " << static_cast<int>(state.value) << '\n';
    else std::cerr << state.message << '\n';
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  return master->Disconnect() ? 0 : 1;
}
