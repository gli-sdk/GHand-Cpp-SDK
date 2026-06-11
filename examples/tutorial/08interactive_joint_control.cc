#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
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

/**
 * @brief Display joint ID list
 */
void DisplayJointIdList() {
  std::cout << "\nJoint ID List:" << '\n';
  std::cout << "  1: THUMB_PIP,      2: THUMB_MCP,      3: THUMB_SWING,     4: "
               "THUMB_ROTATION"
            << '\n';
  std::cout << "  6: FF_PIP,         7: FF_MCP,         8: FF_SWING"
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
              << std::setw(10) << params.speed << std::setw(10) << params.torque
              << '\n';
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

  try {
    return std::stof(input);
  } catch (const std::exception&) {
    std::cout << "  Invalid input, using default value: " << default_value
              << '\n';
    return default_value;
  }
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

  try {
    return std::stoi(input);
  } catch (const std::exception&) {
    std::cout << "  Invalid input, using default value: " << default_value
              << '\n';
    return default_value;
  }
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

int main() {
  std::cout << "========================================" << '\n';
  std::cout << "  GHand Dexterous Hand SDK - Interactive Joint Control    "
            << '\n';
  std::cout << "========================================" << '\n';

  auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                      ghand::CommType::CANFD);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << '\n';
    return -1;
  }

  // Connect device
  std::cout << "\nConnecting to dexterous hand via CANFD..." << '\n';
  bool success = hand->AutoConnect();

  if (!success) {
    std::cerr << "Error: Unable to connect to dexterous hand!" << '\n';
    return 1;
  }

  std::cout << "✓ Successfully connected to dexterous hand!" << '\n';

  // Display device information
  ghand::DeviceInfo info = hand->GetDeviceInfo();
  ghand::HandType hand_type = hand->GetHandType();
  std::cout << "\nDevice Information:" << '\n';
  std::cout << "  Device Name: " << info.device_name << '\n';
  std::cout << "  Hardware Version: " << info.hardware_version << '\n';
  std::cout << "  Software Version: " << info.software_version << '\n';
  std::cout << "  Serial Number: " << info.serial_number << '\n';
  std::cout << "  Hand Type: " << ghand::ToString(hand_type) << '\n';

  // Set control mode to position mode
  hand->SetControlMode(ghand::ControlMode::POSITION);

  // Register joint data callback (for reading feedback)
  std::vector<ghand::Joint> last_joints;
  bool joints_received = false;
  hand->SetJointsCallback([&](const std::vector<ghand::Joint>& joints) {
    last_joints = joints;
    joints_received = true;
  });

  std::cout << "\nInteractive control mode started" << '\n';
  std::cout << "Press Ctrl+C to exit program at any time\n" << '\n';

  try {
    while (true) {
      // Reset joint parameters for each control loop
      std::unordered_map<int, JointParams> joint_params;

      DisplayJointIdList();
      std::cout
          << "\nPlease set joint parameters (press Enter to finish input):\n"
          << '\n';

      // Interactive joint parameter input
      while (true) {
        // Display set joints
        DisplaySetJoints(joint_params);

        std::cout << "\nEnter joint ID (or press Enter to finish input): ";
        std::string joint_input;
        std::getline(std::cin, joint_input);

        // User pressed Enter directly, finish input
        if (joint_input.empty()) {
          break;
        }

        // Parse joint ID
        try {
          int joint_id = std::stoi(joint_input);

          if (!IsValidJointId(joint_id)) {
            std::cout << "  Invalid joint ID, please re-enter" << '\n';
            continue;
          }

          // Get current parameters (if any)
          JointParams& params = joint_params[joint_id];
          std::string joint_name =
              ghand::ToString(static_cast<ghand::JointId>(joint_id));

          std::cout << "\nSetting parameters for joint " << joint_name << ":"
                    << '\n';

          // Read angle, speed, torque
          params.angle =
              ReadFloatWithDefault("Angle value (degrees)", params.angle);
          params.speed =
              ReadIntWithDefault("Speed value (0-100)", params.speed);
          params.torque =
              ReadIntWithDefault("Torque value (0-100)", params.torque);

          std::cout << "  ✓ Set successfully" << '\n';

        } catch (const std::exception& e) {
          std::cout << "  Input format error: " << e.what() << '\n';
        }
      }

      // Build joint command list
      std::vector<ghand::JointCommand> joints;

      if (joint_params.empty()) {
        std::cout
            << "\nNo joints set, all joints will maintain current position"
            << '\n';
      } else {
        for (const auto& pair : joint_params) {
          int joint_id = pair.first;
          const JointParams& params = pair.second;

          // Use angle directly (degrees)
          joints.push_back({static_cast<ghand::JointId>(joint_id),
                            params.angle,
                            static_cast<int8_t>(params.speed),
                            static_cast<int8_t>(params.torque)});
        }

        std::cout << "\nSending joint commands..." << '\n';
      }

      // Send joint commands
      bool move_success = hand->MoveJoints(joints);

      if (move_success) {
        std::cout << "✓ Command sent successfully" << '\n';

        // Wait for device to respond and get joint data
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Display current joint status (if callback data available)
        if (joints_received && !last_joints.empty()) {
          std::cout << "\nCurrent Joint Status (partial display):" << '\n';
          std::cout << std::left << std::setw(20) << "Joint" << std::setw(12)
                    << "Angle(deg)"
                    << "State" << '\n';
          std::cout << std::string(40, '-') << '\n';

          // Only display first 5 joints as example
          for (size_t i = 0; i < std::min(size_t(5), last_joints.size()); ++i) {
            const ghand::Joint& joint = last_joints[i];
            // GetJoints() returns angles in degrees, no conversion needed
            std::cout << std::left << std::setw(20)
                      << ghand::ToString(joint.id) << std::fixed
                      << std::setprecision(1) << std::setw(12)
                      << joint.angle << ghand::ToString(joint.state)
                      << '\n';
          }
          if (last_joints.size() > 5) {
            std::cout << "... (and " << (last_joints.size() - 5)
                      << " more joints)" << '\n';
          }
        }
      } else {
        std::cerr << "✗ Command send failed" << '\n';
      }

      std::cout << "\n" << std::string(50, '=') << '\n';
      std::cout << "Press Enter to start next control cycle..." << '\n';
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

  } catch (const std::exception& e) {
    std::cout << "\nProgram exception: " << e.what() << '\n';
  }

  // Disconnect
  std::cout << "\nDisconnecting..." << '\n';
  hand->Disconnect();
  std::cout << "✓ Disconnected" << '\n';

  std::cout << "\nProgram ended. Thank you for using!" << '\n';
  return 0;
}
