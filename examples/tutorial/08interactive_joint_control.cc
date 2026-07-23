#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

#include "ghand/ghand.h"

/**
 * @brief Joint parameter structure
 */
struct JointParams {
  float angle;  // Angle (degrees)
  int speed;    // Speed (0-100)
  int torque;   // Torque (0-100)

  JointParams() : angle(0), speed(0), torque(0) {}
};

bool ParseFloat(const std::string& input, float* value) {
  if (value == nullptr) return false;
  errno = 0;
  char* end = nullptr;
  float parsed = std::strtof(input.c_str(), &end);
  if (end == input.c_str() || errno == ERANGE) return false;
  *value = parsed;
  return true;
}

bool ParseInt(const std::string& input, int* value) {
  if (value == nullptr) return false;
  errno = 0;
  char* end = nullptr;
  long parsed = std::strtol(input.c_str(), &end, 10);
  if (end == input.c_str() || errno == ERANGE ||
      parsed < (std::numeric_limits<int>::min)() ||
      parsed > (std::numeric_limits<int>::max)()) {
    return false;
  }
  *value = static_cast<int>(parsed);
  return true;
}

/**
 * @brief Display joint ID list
 */
void DisplayJointIdList() {
  std::cout << "\nJoint ID List:" << '\n';
  std::cout << "  1: THUMB_MCP,      2: THUMB_TMC_FE,      "
               "3: THUMB_TMC_AA,     4: THUMB_TMC_PS"
            << '\n';
  std::cout << "  6: FF_PIP,         7: FF_MCP,         8: FF_MCP_AA"
            << '\n';
  std::cout << " 10: MF_PIP,        11: MF_MCP" << '\n';
  std::cout << " 13: RF_PIP,        14: RF_MCP" << '\n';
  std::cout << " 16: LF_PIP,        17: LF_MCP" << '\n';
}

/**
 * @brief Display set joint parameters
 */
void DisplaySetJoints(
    const std::unordered_map<int, JointParams>& joint_params) {
  if (joint_params.empty()) {
    return;
  }

  std::cout << "\nSet Joints:" << '\n';
  std::cout << std::left << std::setw(20) << "Joint Name" << std::setw(10)
            << "Angle(deg)" << std::setw(10) << "Speed(%)" << std::setw(10)
            << "Torque(%)" << '\n';
  std::cout << std::string(50, '-') << '\n';

  for (const auto& pair : joint_params) {
    int joint_id = pair.first;
    const JointParams& params = pair.second;

    // Get joint name
    std::string joint_name =
        ghand::ToString(static_cast<ghand::JointId>(joint_id));

    std::cout << std::left << std::setw(20) << joint_name << std::fixed
              << std::setprecision(1) << std::setw(10) << params.angle
              << std::setw(10) << params.speed << std::setw(10)
              << params.torque << '\n';
  }
}

/**
 * @brief Read numeric value from user with default value support
 * @param prompt Prompt message
 * @param default_value Default value
 * @return User input value, or default value if user presses Enter directly
 */
float ReadFloatWithDefault(const std::string& prompt, float default_value) {
  std::cout << prompt << " [" << default_value << "]: ";
  std::string input;
  std::getline(std::cin, input);

  if (input.empty()) {
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

/**
 * @brief Read integer from user with default value support
 */
int ReadIntWithDefault(const std::string& prompt, int default_value) {
  std::cout << prompt << " [" << default_value << "]: ";
  std::string input;
  std::getline(std::cin, input);

  if (input.empty()) {
    return default_value;
  }

  int value = default_value;
  if (!ParseInt(input, &value)) {
    std::cout << "  Invalid input, using default value: " << default_value
              << '\n';
    return default_value;
  }
  return value;
}

/**
 * @brief Check if joint ID is valid
 */
bool IsValidJointId(int id) {
  // List of valid joint IDs
  static const int kValidIds[] = {
      1,  2,  3, 4,  // THUMB
      6,  7,  8,     // FF (Forefinger)
      10, 11,        // MF (Middle finger)
      13, 14,        // RF (Ring finger)
      16, 17         // LF (Little finger)
  };

  for (int valid_id : kValidIds) {
    if (id == valid_id) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<ghand::DexHand> CreateConnectedHand() {
  auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                      ghand::CommType::ETHERCAT);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << '\n';
    return nullptr;
  }

  std::cout << "\nConnecting to dexterous hand..." << '\n';
  if (!hand->AutoConnect()) {
    std::cerr << "Error: Unable to connect to dexterous hand!" << '\n';
    return nullptr;
  }

  std::cout << "OK Successfully connected to dexterous hand!" << '\n';
  return hand;
}

void DisplayDeviceInfo(ghand::DexHand& hand) {
  ghand::DeviceInfo info = hand.GetDeviceInfo();
  ghand::HandType hand_type = hand.GetHandType();
  std::cout << "\nDevice Information:" << '\n';
  std::cout << "  Device Name: " << info.device_name << '\n';
  std::cout << "  Hardware Version: " << info.hardware_version << '\n';
  std::cout << "  Software Version: " << info.software_version << '\n';
  std::cout << "  Serial Number: " << info.serial_number << '\n';
  std::cout << "  Hand Type: " << ghand::ToString(hand_type) << '\n';
}

bool ReadJointIdInput(int* joint_id, bool* done) {
  std::cout << "\nEnter joint ID (or press Enter to finish input): ";
  std::string joint_input;
  std::getline(std::cin, joint_input);

  if (joint_input.empty()) {
    *done = true;
    return false;
  }

  if (!ParseInt(joint_input, joint_id)) {
    std::cout << "  Input format error, please re-enter" << '\n';
    return false;
  }

  if (!IsValidJointId(*joint_id)) {
    std::cout << "  Invalid joint ID, please re-enter" << '\n';
    return false;
  }
  return true;
}

std::unordered_map<int, JointParams> ReadJointParams() {
  std::unordered_map<int, JointParams> joint_params;

  while (true) {
    DisplaySetJoints(joint_params);

    int joint_id = 0;
    bool done = false;
    if (!ReadJointIdInput(&joint_id, &done)) {
      if (done) break;
      continue;
    }

    JointParams& params = joint_params[joint_id];
    std::string joint_name =
        ghand::ToString(static_cast<ghand::JointId>(joint_id));
    std::cout << "\nSetting parameters for joint " << joint_name << ":"
              << '\n';
    params.angle =
        ReadFloatWithDefault("Angle value (degrees)", params.angle);
    params.speed = ReadIntWithDefault("Speed value (0-100)", params.speed);
    params.torque =
        ReadIntWithDefault("Torque value (0-100)", params.torque);
    std::cout << "  OK Set successfully" << '\n';
  }

  return joint_params;
}

std::vector<ghand::JointCommand> BuildJointCommands(
    const std::unordered_map<int, JointParams>& joint_params) {
  std::vector<ghand::JointCommand> joints;
  for (const auto& pair : joint_params) {
    int joint_id = pair.first;
    const JointParams& params = pair.second;
    joints.push_back({static_cast<ghand::JointId>(joint_id),
                      params.angle,
                      static_cast<int8_t>(params.speed),
                      static_cast<int8_t>(params.torque)});
  }
  return joints;
}

void DisplayJointFeedback(const std::vector<ghand::Joint>& last_joints) {
  std::cout << "\nCurrent Joint Status (partial display):" << '\n';
  std::cout << std::left << std::setw(20) << "Joint" << std::setw(12)
            << "Angle(deg)"
            << "State" << '\n';
  std::cout << std::string(40, '-') << '\n';

  for (size_t i = 0; i < std::min(size_t(5), last_joints.size()); ++i) {
    const ghand::Joint& joint = last_joints[i];
    std::cout << std::left << std::setw(20) << ghand::ToString(joint.id)
              << std::fixed << std::setprecision(1) << std::setw(12)
              << joint.angle << ghand::ToString(joint.state) << '\n';
  }
  if (last_joints.size() > 5) {
    std::cout << "... (and " << (last_joints.size() - 5)
              << " more joints)" << '\n';
  }
}

void SendAndDisplayFeedback(ghand::DexHand& hand,
                            const std::vector<ghand::JointCommand>& joints,
                            bool joints_received,
                            const std::vector<ghand::Joint>& last_joints) {
  bool move_success = hand.MoveJoints(joints);
  if (!move_success) {
    std::cerr << "ERROR Command send failed" << '\n';
    return;
  }

  std::cout << "OK Command sent successfully" << '\n';
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  if (joints_received && !last_joints.empty()) {
    DisplayJointFeedback(last_joints);
  }
}

void RunControlLoop(ghand::DexHand& hand,
                    const std::vector<ghand::Joint>& last_joints,
                    bool& joints_received) {
  while (true) {
    DisplayJointIdList();
    std::cout
        << "\nPlease set joint parameters (press Enter to finish input):\n"
        << '\n';

    auto joint_params = ReadJointParams();
    if (joint_params.empty()) {
      std::cout
          << "\nNo joints set, all joints will maintain current position"
          << '\n';
    } else {
      std::cout << "\nSending joint commands..." << '\n';
    }

    std::vector<ghand::JointCommand> joints =
        BuildJointCommands(joint_params);
    SendAndDisplayFeedback(hand, joints, joints_received, last_joints);

    std::cout << "\n" << std::string(50, '=') << '\n';
    std::cout << "Press Enter to start next control cycle..." << '\n';
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  }
}

int main() {
  std::cout << "========================================" << '\n';
  std::cout << "  GHand Dexterous Hand SDK - Interactive Joint Control    "
            << '\n';
  std::cout << "========================================" << '\n';

  auto hand = CreateConnectedHand();
  if (!hand) return 1;

  DisplayDeviceInfo(*hand);
  hand->SetControlMode(ghand::ControlMode::POSITION);

  std::vector<ghand::Joint> last_joints;
  bool joints_received = false;
  hand->SetJointsCallback([&](const std::vector<ghand::Joint>& joints) {
    last_joints = joints;
    joints_received = true;
  });

  std::cout << "\nInteractive control mode started" << '\n';
  std::cout << "Press Ctrl+C to exit program at any time\n" << '\n';
  RunControlLoop(*hand, last_joints, joints_received);

  std::cout << "\nDisconnecting..." << '\n';
  hand->Disconnect();
  std::cout << "OK Disconnected" << '\n';

  std::cout << "\nProgram ended. Thank you for using!" << '\n';
  return 0;
}
