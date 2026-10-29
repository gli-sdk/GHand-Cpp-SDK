// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>
#include <string>

#include "ghand/ghand.h"

// Gesture type definitions.

enum class GestureType {
  OPEN_HAND,
  FIST,
  OK,
  THUMBS_UP,
  SIX_SIGN
};

std::string GetGestureName(GestureType gesture) {
  switch (gesture) {
    case GestureType::OPEN_HAND:
      return "Open Hand";
    case GestureType::FIST:
      return "Fist";
    case GestureType::OK:
      return "OK Gesture";
    case GestureType::THUMBS_UP:
      return "Thumbs Up";
    case GestureType::SIX_SIGN:
      return "Six Sign";
    default:
      return "Unknown";
  }
}

// Gesture definitions.

const std::unordered_map<GestureType,
                         std::unordered_map<ghand::JointId, float>>
    GESTURE_DEFINITIONS = {
        {GestureType::OPEN_HAND,
         {
             {ghand::JointId::THUMB_MCP, 0.0f},
             {ghand::JointId::THUMB_TMC_FE, 0.0f},
             {ghand::JointId::THUMB_TMC_AA, 20.0f},
             {ghand::JointId::THUMB_TMC_PS, 0.0f},
             {ghand::JointId::FF_PIP, 0.0f},
             {ghand::JointId::FF_MCP, 0.0f},
             {ghand::JointId::FF_MCP_AA, 0.0f},
             {ghand::JointId::MF_PIP, 0.0f},
             {ghand::JointId::MF_MCP, 0.0f},
             {ghand::JointId::RF_PIP, 0.0f},
             {ghand::JointId::RF_MCP, 0.0f},
             {ghand::JointId::LF_PIP, 0.0f},
             {ghand::JointId::LF_MCP, 0.0f},
         }},
        {GestureType::FIST,
         {
             {ghand::JointId::THUMB_MCP, 40.0f},
             {ghand::JointId::THUMB_TMC_FE, 30.0f},
             {ghand::JointId::THUMB_TMC_AA, 30.0f},
             {ghand::JointId::THUMB_TMC_PS, 4.0f},
             {ghand::JointId::FF_PIP, 65.0f},
             {ghand::JointId::FF_MCP, 55.0f},
             {ghand::JointId::FF_MCP_AA, 0.0f},
             {ghand::JointId::MF_PIP, 65.0f},
             {ghand::JointId::MF_MCP, 55.0f},
             {ghand::JointId::RF_PIP, 65.0f},
             {ghand::JointId::RF_MCP, 55.0f},
             {ghand::JointId::LF_PIP, 65.0f},
             {ghand::JointId::LF_MCP, 55.0f},
         }},
        {GestureType::OK,
         {
             {ghand::JointId::THUMB_MCP, 40.0f},
             {ghand::JointId::THUMB_TMC_FE, 30.0f},
             {ghand::JointId::THUMB_TMC_AA, 30.0f},
             {ghand::JointId::THUMB_TMC_PS, 4.0f},
             {ghand::JointId::FF_PIP, 30.0f},
             {ghand::JointId::FF_MCP, 50.0f},
             {ghand::JointId::FF_MCP_AA, 0.0f},
             {ghand::JointId::MF_PIP, 0.0f},
             {ghand::JointId::MF_MCP, 0.0f},
             {ghand::JointId::RF_PIP, 0.0f},
             {ghand::JointId::RF_MCP, 0.0f},
             {ghand::JointId::LF_PIP, 0.0f},
             {ghand::JointId::LF_MCP, 0.0f},
         }},
        {GestureType::THUMBS_UP,
         {
             {ghand::JointId::THUMB_MCP, 0.0f},
             {ghand::JointId::THUMB_TMC_FE, 0.0f},
             {ghand::JointId::THUMB_TMC_AA, 20.0f},
             {ghand::JointId::THUMB_TMC_PS, 0.0f},
             {ghand::JointId::FF_PIP, 65.0f},
             {ghand::JointId::FF_MCP, 55.0f},
             {ghand::JointId::FF_MCP_AA, 0.0f},
             {ghand::JointId::MF_PIP, 65.0f},
             {ghand::JointId::MF_MCP, 55.0f},
             {ghand::JointId::RF_PIP, 65.0f},
             {ghand::JointId::RF_MCP, 55.0f},
             {ghand::JointId::LF_PIP, 65.0f},
             {ghand::JointId::LF_MCP, 55.0f},
         }},
        {GestureType::SIX_SIGN,
         {
             {ghand::JointId::THUMB_MCP, 0.0f},
             {ghand::JointId::THUMB_TMC_FE, 0.0f},
             {ghand::JointId::THUMB_TMC_AA, 20.0f},
             {ghand::JointId::THUMB_TMC_PS, 0.0f},
             {ghand::JointId::FF_PIP, 65.0f},
             {ghand::JointId::FF_MCP, 55.0f},
             {ghand::JointId::FF_MCP_AA, 0.0f},
             {ghand::JointId::MF_PIP, 65.0f},
             {ghand::JointId::MF_MCP, 55.0f},
             {ghand::JointId::RF_PIP, 65.0f},
             {ghand::JointId::RF_MCP, 55.0f},
             {ghand::JointId::LF_PIP, 0.0f},
             {ghand::JointId::LF_MCP, 0.0f},
         }},
};

// Global state for error handling.

ghand::HandData g_hand_state;
std::vector<ghand::JointData> g_joints;
std::mutex g_state_mutex;

void OnHandStateUpdate(const ghand::HandData& state) {
  std::lock_guard<std::mutex> lock(g_state_mutex);
  g_hand_state = state;
}

void OnJointsUpdate(const std::vector<ghand::JointData>& joints) {
  std::lock_guard<std::mutex> lock(g_state_mutex);
  g_joints = joints;
}

bool HandHasError(const ghand::HandData& hand) {
  for (uint8_t code : hand.error_warn) if (code) return true;
  for (uint8_t code : hand.error_fault) if (code) return true;
  for (uint8_t code : hand.error_critical) if (code) return true;
  return hand.joints_state != ghand::HandControlState::DISABLED &&
         hand.joints_state != ghand::HandControlState::ENABLED;
}

bool JointHasError(const ghand::JointData& joint) {
  return joint.error != ghand::JointErrorCode::NORMAL ||
         (joint.state != ghand::JointState::DISABLED &&
          joint.state != ghand::JointState::RUNNING &&
          joint.state != ghand::JointState::STOPPED);
}

bool HasError() {
  std::lock_guard<std::mutex> lock(g_state_mutex);

  // Check hand state first (priority 1)
  if (HandHasError(g_hand_state)) {
    return true;
  }

  // Check joints (priority 2)
  for (const auto& joint : g_joints) {
    if (JointHasError(joint)) {
      return true;
    }
  }

  return false;
}

void PrintError() {
  std::lock_guard<std::mutex> lock(g_state_mutex);

  // Print hand state errors
  if (HandHasError(g_hand_state)) {
    std::cerr << "\n[ERROR] Hand state error detected!" << '\n';
    std::cerr << "HandControlState: " << ghand::ToString(g_hand_state.joints_state) << '\n';
    std::cerr << "Temperature: " << g_hand_state.temperature << " degC"
              << '\n';
  }

  // Print joint errors
  std::vector<std::string> faulty_joints;
  for (const auto& joint : g_joints) {
    if (JointHasError(joint)) {
      std::stringstream ss;
      ss << "  - " << ghand::ToString(joint.id)
         << ": state=" << ghand::ToString(joint.state)
         << ", error=" << ghand::ToString(joint.error);
      faulty_joints.push_back(ss.str());
    }
  }

  if (!faulty_joints.empty()) {
    if (HandHasError(g_hand_state)) {
      std::cerr << '\n';
    }
    std::cerr << "[ERROR] Detected " << faulty_joints.size()
              << " faulty joint(s)" << '\n';
    std::cerr << "Faulty joints:" << '\n';
    for (const auto& fault : faulty_joints) {
      std::cerr << fault << '\n';
    }
  }
}

// Helper functions.

std::vector<ghand::JointCommand> CreateJointsFromGesture(
    const std::unordered_map<ghand::JointId, float>& gesture_def) {
  std::vector<ghand::JointCommand> joints;
  for (const auto& pair : gesture_def) {
    joints.push_back({pair.first, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, pair.second, 100, 100});
  }
  return joints;
}

std::unique_ptr<ghand::Master> CreateConnectedHand() {
  auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                     ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create GHand" << '\n';
    return nullptr;
  }

  std::cout << "\nConnecting to dexterous hand..." << '\n';
  const auto connected = master->Connect("auto");
  if (!connected) {
    std::cerr << "Failed to connect: " << connected.message << '\n';
    return nullptr;
  }

  std::cout << "Connected successfully" << '\n';
  // Data callbacks belong to one hand, so register on the hand itself.
  ghand::GHand* hand = connected.value.front();
  hand->SetJointsCallback(OnJointsUpdate);
  hand->SetHandStateCallback(OnHandStateUpdate);
  return master;
}

bool WaitWithErrorCheck(int wait_ms, int check_interval_ms) {
  int elapsed = 0;
  while (elapsed < wait_ms) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(check_interval_ms));
    elapsed += check_interval_ms;
    if (HasError()) return true;
  }
  return false;
}

bool ExecuteGesture(ghand::GHand& hand, GestureType gesture,
                    int gesture_wait_ms, int check_interval_ms) {
  if (HasError()) return true;

  std::cout << "\nExecuting: " << GetGestureName(gesture) << '\n';
  auto it = GESTURE_DEFINITIONS.find(gesture);
  if (it != GESTURE_DEFINITIONS.end()) {
    auto joints = CreateJointsFromGesture(it->second);
    hand.MoveJoints(joints);
  }
  return WaitWithErrorCheck(gesture_wait_ms, check_interval_ms);
}

bool RunGestureCycle(ghand::GHand& hand,
                     const std::vector<GestureType>& gestures,
                     int cycle,
                     int gesture_wait_ms,
                     int cycle_delay_ms,
                     int check_interval_ms) {
  std::cout << "\n========== Cycle " << cycle << " ==========" << '\n';

  for (auto gesture : gestures) {
    if (ExecuteGesture(hand, gesture, gesture_wait_ms, check_interval_ms)) {
      return true;
    }
  }

  std::cout << "\n========== Cycle " << cycle
            << " completed ==========" << '\n';
  std::cout << "Press Ctrl+C to stop, or continue to next cycle...\n"
            << '\n';
  return WaitWithErrorCheck(cycle_delay_ms, check_interval_ms);
}

bool RunGestureDemo(ghand::GHand& hand) {
  const std::vector<GestureType> gestures = {
      GestureType::OPEN_HAND, GestureType::FIST,     GestureType::OK,
      GestureType::THUMBS_UP, GestureType::SIX_SIGN,
  };
  const int kGestureWaitMs = 1500;
  const int kCycleDelayMs = 500;
  const int kErrorCheckIntervalMs = 100;

  std::cout << "\nStarting gesture demonstration..." << '\n';
  std::cout << "Press Ctrl+C to stop\n" << '\n';

  int cycle = 0;
  while (true) {
    cycle++;
    if (RunGestureCycle(hand, gestures, cycle, kGestureWaitMs,
                        kCycleDelayMs, kErrorCheckIntervalMs)) {
      return true;
    }
  }
}

void HandleDetectedError(ghand::GHand& hand) {
  PrintError();
  std::cerr << "\nStopping all motion and clearing fault..." << '\n';
  hand.Stop();
  hand.ClearFault();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

int main() {
  std::cout << "========================================" << '\n';
  std::cout << "  GHand Dexterous Hand SDK - Preset Gesture Demo" << '\n';
  std::cout << "========================================" << '\n';

  auto master = CreateConnectedHand();
  if (!master) return 1;
  auto* hand = master->hands().front();

  bool has_error = RunGestureDemo(*hand);
  if (has_error) {
    HandleDetectedError(*hand);
  }

  std::cout << "\nDisconnecting..." << '\n';
  const auto disconnected = master->Disconnect();
  if (!disconnected) {
    std::cerr << "Disconnect failed: " << disconnected.message << '\n';
    return 1;
  }
  std::cout << "Disconnected. Thank you!" << '\n';

  return has_error ? 1 : 0;
}
