// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

// Gesture pose definitions.

// Thumb touches little finger
std::vector<ghand::JointCommand> MakeThumbTouchLittle() {
  return {
      {ghand::JointId::THUMB_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 50.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 60.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 56.0f, 100, 100},
      {ghand::JointId::LF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 28.0f, 100, 100},
  };
}

// Thumb touches ring finger
std::vector<ghand::JointCommand> MakeThumbTouchRing() {
  return {
      {ghand::JointId::THUMB_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 19.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 38.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 45.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 67.0f, 100, 100},
      {ghand::JointId::RF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 39.0f, 100, 100},
      {ghand::JointId::LF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
  };
}

// Thumb touches middle finger
std::vector<ghand::JointCommand> MakeThumbTouchMiddle() {
  return {
      {ghand::JointId::THUMB_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 17.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 27.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 40.0f, 100, 100},
      {ghand::JointId::MF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 57.0f, 100, 100},
      {ghand::JointId::RF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
  };
}

// Thumb touches index finger
std::vector<ghand::JointCommand> MakeThumbTouchIndex() {
  return {
      {ghand::JointId::THUMB_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 13.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 14.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 51.0f, 100, 100},
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 46.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
  };
}

// Fist pose
std::vector<ghand::JointCommand> MakeFist() {
  return {
      {ghand::JointId::THUMB_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 40.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 65.0f, 100, 100},
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 55.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 65.0f, 100, 100},
      {ghand::JointId::MF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 55.0f, 100, 100},
      {ghand::JointId::RF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 65.0f, 100, 100},
      {ghand::JointId::RF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 55.0f, 100, 100},
      {ghand::JointId::LF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 65.0f, 100, 100},
      {ghand::JointId::LF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 55.0f, 100, 100},
  };
}

// Open index finger
std::vector<ghand::JointCommand> MakeOpenIndexFinger() {
  return {
      {ghand::JointId::THUMB_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 75.0f, 100, 100},
      {ghand::JointId::MF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 70.0f, 100, 100},
      {ghand::JointId::RF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 75.0f, 100, 100},
      {ghand::JointId::RF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 70.0f, 100, 100},
      {ghand::JointId::LF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 70.0f, 100, 100},
      {ghand::JointId::LF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 70.0f, 100, 100},
  };
}

// Open middle finger
std::vector<ghand::JointCommand> MakeOpenMiddleFinger() {
  return {
      {ghand::JointId::THUMB_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 75.0f, 100, 100},
      {ghand::JointId::RF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 70.0f, 100, 100},
      {ghand::JointId::LF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 70.0f, 100, 100},
      {ghand::JointId::LF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 70.0f, 100, 100},
  };
}

// Open hand pose
std::vector<ghand::JointCommand> MakeOpenHand() {
  return {
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
}

// OK pose
std::vector<ghand::JointCommand> MakeOK() {
  return {
      {ghand::JointId::THUMB_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 40.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 30.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 30.0f, 100, 100},
      {ghand::JointId::FF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 50.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, ghand::JointControlMode::POSITION, ghand::JointLoopType::CLOSED_LOOP, ghand::JointControlState::RUNNING, 0.0f, 100, 100},
  };
}

// Action execution functions.

bool ResetToOpenHand(ghand::GHand& hand) {
  auto joints = MakeOpenHand();
  return hand.MoveJoints(joints);
}

bool ThumbTouch(ghand::GHand& hand) {
  const std::vector<std::vector<ghand::JointCommand>> poses = {
      MakeThumbTouchLittle(), MakeThumbTouchRing(), MakeThumbTouchMiddle(),
      MakeThumbTouchIndex()};
  for (const auto& pose : poses) {
    if (!hand.MoveJoints(pose)) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return ResetToOpenHand(hand);
}

bool FistThenOpen(ghand::GHand& hand) {
  if (!hand.MoveJoints(MakeFist())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return ResetToOpenHand(hand);
}

bool SeqOpenFinger(ghand::GHand& hand) {
  if (!hand.MoveJoints(MakeFist())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (!hand.MoveJoints(MakeOpenIndexFinger())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (!hand.MoveJoints(MakeOpenMiddleFinger())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return ResetToOpenHand(hand);
}

bool MakeOKGesture(ghand::GHand& hand) {
  if (!hand.MoveJoints(MakeOK())) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return ResetToOpenHand(hand);
}

// First action group.
bool FirstAction(ghand::GHand& hand) {
  if (!ThumbTouch(hand)) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  for (int i = 0; i < 2; i++) {
    if (!FistThenOpen(hand)) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return true;
}

// Second action group.
bool SecondAction(ghand::GHand& hand) {
  if (!SeqOpenFinger(hand)) return false;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (!MakeOKGesture(hand)) return false;
  return true;
}

int main() {
  std::cout << "***** GHand SDK - Gesture Dance Demo *****\n\n";
  auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create GHand\n";
    return 1;
  }

  const auto connected = master->Connect("auto");
  if (!connected) {
    std::cerr << "Failed to connect to dexterous hand: " << connected.message
              << '\n';
    return 1;
  }
  auto* hand = connected.value.front();
  std::cout << "\n--- Device ready, starting gesture dance demo ---\n";
  std::cout << "Press Ctrl+C to stop demo and exit\n\n";

  bool failed = false;
  int round = 0;
  while (true) {
    ++round;
    std::cout << "\n--- Round " << round << " gesture demo started ---\n";

    if (!FirstAction(*hand)) {
      std::cerr << "Round " << round << " first action group failed\n";
      failed = true;
      break;
    }

    if (!SecondAction(*hand)) {
      std::cerr << "Round " << round << " second action group failed\n";
      failed = true;
      break;
    }

    std::cout << "--- Round " << round << " gesture demo finished ---\n";
  }

  if (failed) {
    const auto stopped = hand->Stop();
    if (!stopped) {
      std::cerr << "Stop failed: " << stopped.message << '\n';
    }
  }

  const auto disconnected = master->Disconnect();
  if (!disconnected) {
    std::cerr << "Disconnect failed: " << disconnected.message << '\n';
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "\n--- Demo finished, disconnected ---\n";
  return failed ? 1 : 0;
}
