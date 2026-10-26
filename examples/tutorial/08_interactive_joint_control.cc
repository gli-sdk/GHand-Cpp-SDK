// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ghand/ghand.h"

namespace {

struct JointParams {
  float angle = 0.0f;
  int speed = 0;
  int current = 0;
};

// DIP joints are not controllable on GHand5, so they are not offered here.
constexpr ghand::JointId kValidJoints[] = {
    ghand::JointId::THUMB_MCP,    ghand::JointId::THUMB_TMC_FE,
    ghand::JointId::THUMB_TMC_AA, ghand::JointId::THUMB_TMC_PS,
    ghand::JointId::FF_PIP,       ghand::JointId::FF_MCP,
    ghand::JointId::FF_MCP_AA,    ghand::JointId::MF_PIP,
    ghand::JointId::MF_MCP,       ghand::JointId::RF_PIP,
    ghand::JointId::RF_MCP,       ghand::JointId::LF_PIP,
    ghand::JointId::LF_MCP,
};

bool ParseFloat(const std::string& input, float* value) {
  if (value == nullptr) return false;
  errno = 0;
  char* end = nullptr;
  const float parsed = std::strtof(input.c_str(), &end);
  if (end == input.c_str() || errno == ERANGE || !std::isfinite(parsed)) {
    return false;
  }
  *value = parsed;
  return true;
}

bool ParseInt(const std::string& input, int* value) {
  if (value == nullptr) return false;
  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(input.c_str(), &end, 10);
  if (end == input.c_str() || errno == ERANGE ||
      parsed < (std::numeric_limits<int>::min)() ||
      parsed > (std::numeric_limits<int>::max)()) {
    return false;
  }
  *value = static_cast<int>(parsed);
  return true;
}

void DisplayJointIdList() {
  std::cout << "\nJoint ID List (DIP joints are not controllable):\n";
  for (ghand::JointId id : kValidJoints) {
    std::cout << "  " << std::setw(2) << static_cast<int>(id) << ": "
              << ghand::ToString(id) << '\n';
  }
}

void DisplaySetJoints(
    const std::unordered_map<int, JointParams>& joint_params) {
  if (joint_params.empty()) {
    return;
  }

  std::cout << "\nSet Joints:\n";
  std::cout << std::left << std::setw(20) << "Joint Name" << std::setw(10)
            << "Angle(deg)" << std::setw(10) << "Speed(%)" << std::setw(10)
            << "Current(%)" << '\n';
  std::cout << std::string(50, '-') << '\n';

  for (const auto& pair : joint_params) {
    const int joint_id = pair.first;
    const JointParams& params = pair.second;
    std::cout << std::left << std::setw(20)
              << ghand::ToString(static_cast<ghand::JointId>(joint_id))
              << std::fixed << std::setprecision(1) << std::setw(10)
              << params.angle << std::setw(10) << params.speed << std::setw(10)
              << params.current << '\n';
  }
}

float ReadFloatWithDefault(const std::string& prompt, float default_value) {
  std::cout << prompt << " [" << default_value << "]: ";
  std::string input;
  if (!std::getline(std::cin, input) || input.empty()) {
    return default_value;
  }

  float value = default_value;
  if (!ParseFloat(input, &value)) {
    std::cout << "  Invalid input, using default value: " << default_value
              << '\n';
    return default_value;
  }
  return value;
}

int ReadIntInRange(const std::string& prompt, int default_value, int min_value,
                   int max_value) {
  while (true) {
    std::cout << prompt << " [" << default_value << "]: ";
    std::string input;
    if (!std::getline(std::cin, input) || input.empty()) {
      return default_value;
    }

    int value = 0;
    if (ParseInt(input, &value) && value >= min_value && value <= max_value) {
      return value;
    }
    std::cout << "  Invalid input, expected " << min_value << "-" << max_value
              << ", please re-enter\n";
  }
}

bool IsValidJointId(int id) {
  for (ghand::JointId valid : kValidJoints) {
    if (id == static_cast<int>(valid)) {
      return true;
    }
  }
  return false;
}

enum class JointIdInput { kValue, kDone, kEof, kRetry };

JointIdInput ReadJointIdInput(int* joint_id) {
  std::cout << "\nEnter joint ID (or press Enter to finish input): ";
  std::string joint_input;
  if (!std::getline(std::cin, joint_input)) {
    return JointIdInput::kEof;
  }
  if (joint_input.empty()) {
    return JointIdInput::kDone;
  }

  if (!ParseInt(joint_input, joint_id) || !IsValidJointId(*joint_id)) {
    std::cout << "  Invalid joint ID, please re-enter\n";
    return JointIdInput::kRetry;
  }
  return JointIdInput::kValue;
}

// Returns false when stdin is closed; an empty map means the user pressed
// Enter without selecting any joint.
bool ReadJointParams(std::unordered_map<int, JointParams>* joint_params) {
  while (true) {
    DisplaySetJoints(*joint_params);

    int joint_id = 0;
    const JointIdInput input = ReadJointIdInput(&joint_id);
    if (input == JointIdInput::kEof) return false;
    if (input == JointIdInput::kDone) return true;
    if (input == JointIdInput::kRetry) continue;

    JointParams& params = (*joint_params)[joint_id];
    std::cout << "\nSetting parameters for joint "
              << ghand::ToString(static_cast<ghand::JointId>(joint_id))
              << ":\n";
    params.angle =
        ReadFloatWithDefault("Angle value (degrees)", params.angle);
    params.speed = ReadIntInRange("Speed value (0-100)", params.speed, 0, 100);
    params.current =
        ReadIntInRange("Current value (0-100)", params.current, 0, 100);
    std::cout << "  OK Set successfully\n";
  }
}

std::vector<ghand::JointCommand> BuildJointCommands(
    const std::unordered_map<int, JointParams>& joint_params) {
  std::vector<ghand::JointCommand> joints;
  joints.reserve(joint_params.size());
  for (const auto& pair : joint_params) {
    const JointParams& params = pair.second;
    joints.push_back({static_cast<ghand::JointId>(pair.first), params.angle,
                      static_cast<int16_t>(params.speed),
                      static_cast<int16_t>(params.current)});
  }
  return joints;
}

void DisplayJointFeedback(const std::vector<ghand::Joint>& joints) {
  std::cout << "\nCurrent Joint Status:\n";
  std::cout << std::left << std::setw(20) << "Joint" << std::setw(12)
            << "Angle(deg)" << "State" << '\n';
  std::cout << std::string(48, '-') << '\n';

  for (const auto& joint : joints) {
    std::cout << std::left << std::setw(20) << ghand::ToString(joint.id)
              << std::fixed << std::setprecision(1) << std::setw(12)
              << joint.angle << ghand::ToString(joint.state) << '\n';
  }
}

void RunControlLoop(ghand::GHand& hand) {
  while (true) {
    DisplayJointIdList();
    std::cout << "\nPlease set joint parameters (press Enter to finish input):\n";

    std::unordered_map<int, JointParams> joint_params;
    if (!ReadJointParams(&joint_params)) {
      std::cout << "\nInput closed, exiting.\n";
      return;
    }

    if (joint_params.empty()) {
      std::cout << "\nNo joints set, all joints maintain their current position\n";
    } else {
      std::cout << "\nSending joint commands...\n";
      const std::vector<ghand::JointCommand> joints =
          BuildJointCommands(joint_params);
      const auto result = hand.MoveJoints(joints);
      if (!result) {
        std::cerr << "Command send failed: " << result.message << '\n';
      } else {
        std::cout << "Command sent successfully\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        const auto joint_data = hand.GetJointsData();
        if (joint_data) {
          DisplayJointFeedback(joint_data.value);
        } else {
          std::cerr << "Failed to read joint data: " << joint_data.message
                    << '\n';
        }
      }
    }

    std::cout << "\n" << std::string(50, '=') << '\n';
    std::cout << "Press Enter to start next control cycle...\n";
    std::string line;
    if (!std::getline(std::cin, line)) {
      std::cout << "\nInput closed, exiting.\n";
      return;
    }
  }
}

}  // namespace

int main() {
  std::cout << "========================================\n"
            << "  GHand Dexterous Hand SDK - Interactive Joint Control\n"
            << "========================================\n";

  auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create GHand\n";
    return 1;
  }

  std::cout << "\nConnecting to dexterous hand...\n";
  const auto connected = master->Connect("auto");
  if (!connected) {
    std::cerr << "Unable to connect to dexterous hand: " << connected.message
              << '\n';
    return 1;
  }
  std::cout << "OK Successfully connected to dexterous hand!\n";

  ghand::GHand* hand = connected.value.front();
  const auto info = hand->QueryDeviceInfo();
  if (!info) {
    std::cerr << "Failed to query device info: " << info.message << '\n';
  } else {
    std::cout << "\nDevice Information:\n"
              << "  Device Name: " << info.value.device_name << '\n'
              << "  Software Version: " << info.value.software_version << '\n'
              << "  Serial Number: " << info.value.serial_number << '\n'
              << "  Hand Type: " << ghand::ToString(hand->hand_type()) << '\n';
  }

  std::cout << "\nInteractive control mode started\n"
            << "Press Ctrl+C to exit program at any time\n";
  RunControlLoop(*hand);

  std::cout << "\nDisconnecting...\n";
  const auto disconnected = master->Disconnect();
  if (!disconnected) {
    std::cerr << "Disconnect failed: " << disconnected.message << '\n';
    return 1;
  }
  std::cout << "OK Disconnected\n";
  return 0;
}
