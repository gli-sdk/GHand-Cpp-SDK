// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include <iostream>
#include <thread>

#include "ghand/ghand.h"

int main() {
  auto hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                 ghand::CommType::ETHERCAT);
  if (!hand || !hand->AutoConnect()) return 1;
  hand->SetHandStateCallback([](const ghand::HandState& state) {
    std::cout << ghand::ToString(state) << '\n';
  });
  ghand::HandStateMachine state;
  if (!hand->GetHandStateMachine(&state)) {
    std::cerr << hand->GetLastErrorMessage() << '\n';
    hand->Disconnect();
    return 1;
  }
  std::cout << "Hand state machine: " << static_cast<int>(state) << '\n';
  std::this_thread::sleep_for(std::chrono::seconds(2));
  hand->Disconnect();
  return 0;
}
