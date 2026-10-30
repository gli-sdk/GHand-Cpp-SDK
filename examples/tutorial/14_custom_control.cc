// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// CUSTOM preserves per-joint mode and loop type. Keep each arbitration group
// consistent: FF_PIP/MCP/MCP_AA, MF_PIP/MCP, RF_PIP/MCP, LF_PIP/MCP.
// The four active thumb joints are independent. Inconsistent groups can be
// silently ignored by firmware while previous commands continue executing.
int main() {
  using namespace ghand;
  auto master = Master::Create(ProductType::GHand5, CommType::ETHERCAT);
  if (!master) return 1;
  const auto connected = master->Connect("auto");
  if (!connected || connected.value.empty()) {
    std::cerr << "Connection failed: " << connected.message << '\n';
    return 1;
  }
  auto* hand = connected.value.front();
  int exit_code = 0;
  for (int step = 0; step < 2; ++step) {
    std::vector<JointCommand> commands;
    // Position control needs nonzero velocity/current limits to hold position.
    for (JointId id : {JointId::FF_MCP, JointId::FF_PIP, JointId::FF_MCP_AA}) {
      JointCommand joint(id);
      joint.angle = (step == 0 && id == JointId::FF_MCP) ? 30.0f : 0.0f;
      joint.velocity = 10;
      joint.current = 20;
      commands.push_back(joint);
    }
    JointCommand middle(JointId::MF_MCP, JointControlMode::CURRENT);
    middle.current = step == 0 ? 30 : -30;
    commands.push_back(middle);
    // Disabled PIP still selects CURRENT to match its finger's MCP mode.
    commands.emplace_back(JointId::MF_PIP, JointControlMode::CURRENT,
                          JointLoopType::CLOSED_LOOP, JointControlState::DISABLED);
    const auto sent = hand->MoveJoints(commands, HandControlMode::CUSTOM);
    if (!sent) {
      std::cerr << "CUSTOM command failed: " << sent.message << '\n';
      exit_code = 1;
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
    const auto state = hand->GetHandStateMachine();
    if (state) std::cout << "Hand runtime state: " << static_cast<int>(state.value) << '\n';
    const auto feedback = hand->GetJointsData();
    if (!feedback) {
      std::cerr << "Feedback failed: " << feedback.message << '\n';
      exit_code = 1;
      break;
    }
    for (const auto& joint : feedback.value) {
      if (joint.id != JointId::FF_MCP && joint.id != JointId::MF_MCP) continue;
      std::cout << ToString(joint.id) << ": " << joint.angle << " deg, velocity="
                << joint.velocity << "%, current=" << joint.current
                << "%, mode=" << static_cast<int>(joint.control_mode)
                << ", loop=" << static_cast<int>(joint.loop_type)
                << ", state=" << ToString(joint.state)
                << ", error=" << ToString(joint.error) << '\n';
    }
  }
  // Disconnect closes communication only; explicitly release drive torque.
  const auto disabled = hand->DisableHand();
  if (!disabled) {
    std::cerr << "Disable failed: " << disabled.message << '\n';
    exit_code = 1;
  }
  if (!master->Disconnect()) exit_code = 1;
  return exit_code;
}
