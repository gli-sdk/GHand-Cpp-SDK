// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// GHand5: degrees, velocity/current in percent, kp=10..2000, kd=0..50,
// kp > kd. MIT current is feed-forward, not a current limit.
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
  // Run inside a lambda so every failure reaches the same cleanup below.
  const auto run = [&]() -> bool {
    const auto opened = hand->OpenTactile();
    if (!opened) {
      std::cerr << "Open tactile failed: " << opened.message << '\n';
      return false;
    }
    const JointId ids[] = {JointId::THUMB_MCP, JointId::THUMB_TMC_FE,
        JointId::THUMB_TMC_AA, JointId::THUMB_TMC_PS, JointId::FF_PIP,
        JointId::FF_MCP, JointId::FF_MCP_AA, JointId::MF_PIP,
        JointId::MF_MCP, JointId::RF_PIP, JointId::RF_MCP,
        JointId::LF_PIP, JointId::LF_MCP};
    const float angles[] = {66, 50, 90, 0, 80, 90, 0, 90, 90, 90, 90, 74, 90};
    std::vector<JointCommand> commands;
    for (size_t i = 0; i < 13; ++i) {
      JointCommand joint(ids[i]);
      joint.angle = angles[i];
      joint.velocity = (i == 7 || i == 8) ? 7 : 5;
      joint.kp = 10;
      commands.push_back(joint);
    }
    const auto send = [&]() -> bool {
      const auto result = hand->MoveJoints(commands, HandControlMode::MIT);
      if (!result) std::cerr << "MIT command failed: " << result.message << '\n';
      return static_cast<bool>(result);
    };
    if (!send()) return false;
    // Match Python: contact means at least three fingers have Fz >= 0.8 N.
    bool contact = false;
    for (int i = 0; i < 100; ++i) {
      const auto data = hand->GetTactileData();
      int touching = 0;
      if (data) {
        for (const auto& finger : data.value) {
          if (finger.second.state && finger.second.resultant_force.size() == 3 &&
              finger.second.resultant_force[2] >= 0.8f) ++touching;
        }
      }
      if (touching >= 3) { contact = true; break; }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!contact) {
      std::cout << "No contact within 10 s; disabling hand.\n";
      return true;
    }
    for (auto& joint : commands) { joint.kp = 25; joint.kd = 2; joint.current = 5; }
    if (!send()) return false;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    for (auto& joint : commands) {
      joint.angle = 0; joint.kp = 10; joint.kd = 0; joint.current = 0;
    }
    if (!send()) return false;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    // Success means sent; inspect hand/joint feedback for actual execution.
    const auto feedback = hand->GetJointsData();
    if (!feedback) return false;
    for (const auto& joint : feedback.value)
      std::cout << ToString(joint.id) << ": " << joint.angle << " deg, "
                << ToString(joint.state) << '\n';
    return true;
  };
  if (!run()) exit_code = 1;
  const auto closed = hand->CloseTactile();
  const auto disabled = hand->DisableHand();
  if (!closed || !disabled) {
    std::cerr << "Cleanup: tactile=" << closed.message
              << ", disable=" << disabled.message << '\n';
    exit_code = 1;
  }
  if (!master->Disconnect()) exit_code = 1;
  return exit_code;
}
