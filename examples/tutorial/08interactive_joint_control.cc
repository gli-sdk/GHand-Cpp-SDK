#include "ghand/dexhand.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <string>
#include <sstream>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace ghand;

/**
 * @brief Joint parameter structure
 */
struct JointParams {
    float angle;   // Angle (degrees)
    int speed;     // Speed (0-100)
    int torque;    // Torque (0-100)

    JointParams() : angle(0), speed(0), torque(0) {}
};

/**
 * @brief Display joint ID list
 */
void DisplayJointIdList() {
    std::cout << "\nJoint ID List:" << std::endl;
    std::cout << "  1: THUMB_PIP,      2: THUMB_MCP,      3: THUMB_SWING,     4: THUMB_ROTATION" << std::endl;
    std::cout << "  6: FF_PIP,         7: FF_MCP,         8: FF_SWING" << std::endl;
    std::cout << " 10: MF_PIP,        11: MF_MCP" << std::endl;
    std::cout << " 13: RF_PIP,        14: RF_MCP" << std::endl;
    std::cout << " 16: LF_PIP,        17: LF_MCP" << std::endl;
}

/**
 * @brief Display set joint parameters
 */
void DisplaySetJoints(const std::unordered_map<int, JointParams>& joint_params) {
    if (joint_params.empty()) {
        return;
    }

    std::cout << "\nSet Joints:" << std::endl;
    std::cout << std::left << std::setw(20) << "Joint Name"
              << std::setw(10) << "Angle(deg)"
              << std::setw(10) << "Speed(%)"
              << std::setw(10) << "Torque(%)" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    for (const auto& pair : joint_params) {
        int joint_id = pair.first;
        const JointParams& params = pair.second;

        // Get joint name
        std::string joint_name = ToString(static_cast<JointId>(joint_id));

        std::cout << std::left << std::setw(20) << joint_name
                  << std::fixed << std::setprecision(1) << std::setw(10) << params.angle
                  << std::setw(10) << params.speed
                  << std::setw(10) << params.torque << std::endl;
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
        std::cout << "  Invalid input, using default value: " << default_value << std::endl;
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
        std::cout << "  Invalid input, using default value: " << default_value << std::endl;
        return default_value;
    }
}

/**
 * @brief Check if joint ID is valid
 */
bool IsValidJointId(int id) {
    // List of valid joint IDs
    static const int valid_ids[] = {
        1, 2, 3, 4,     // THUMB
        6, 7, 8,        // FF (Forefinger)
        10, 11,         // MF (Middle finger)
        13, 14,         // RF (Ring finger)
        16, 17          // LF (Little finger)
    };

    for (int valid_id : valid_ids) {
        if (id == valid_id) {
            return true;
        }
    }
    return false;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  GHand Dexterous Hand SDK - Interactive Joint Control    " << std::endl;
    std::cout << "========================================" << std::endl;

    auto hand = DexHand::Create(ProductType::G5, CommType::ETHERCAT);
    if (!hand) {
        std::cerr << "Failed to create DexHand" << std::endl;
        return -1;
    }

    // Connect device
    std::cout << "\nConnecting to dexterous hand via EtherCAT..." << std::endl;
    bool success = hand->AutoConnect();

    if (!success) {
        std::cerr << "Error: Unable to connect to dexterous hand!" << std::endl;
        return 1;
    }

    std::cout << "✓ Successfully connected to dexterous hand!" << std::endl;

    // Display device information
    DeviceInfo info = hand->GetDeviceInfo();
    HandType hand_type = hand->GetHandType();
    std::cout << "\nDevice Information:" << std::endl;
    std::cout << "  Device Name: " << info.device_name << std::endl;
    std::cout << "  Hardware Version: " << info.hardware_version << std::endl;
    std::cout << "  Software Version: " << info.software_version << std::endl;
    std::cout << "  Serial Number: " << info.serial_number << std::endl;
    std::cout << "  Hand Type: " << ToString(hand_type) << std::endl;

    // Set control mode to position mode
    hand->SetControlMode(ControlMode::POSITION);

    // Register joint data callback (for reading feedback)
    std::vector<Joint> last_joints;
    bool joints_received = false;
    hand->SetJointsCallback([&](const std::vector<Joint>& joints) {
        last_joints = joints;
        joints_received = true;
    });

    std::cout << "\nInteractive control mode started" << std::endl;
    std::cout << "Press Ctrl+C to exit program at any time\n" << std::endl;

    try {
        while (true) {
            // Reset joint parameters for each control loop
            std::unordered_map<int, JointParams> joint_params;

            DisplayJointIdList();
            std::cout << "\nPlease set joint parameters (press Enter to finish input):\n" << std::endl;

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
                        std::cout << "  Invalid joint ID, please re-enter" << std::endl;
                        continue;
                    }

                    // Get current parameters (if any)
                    JointParams& params = joint_params[joint_id];
                    std::string joint_name = ToString(static_cast<JointId>(joint_id));

                    std::cout << "\nSetting parameters for joint " << joint_name << ":" << std::endl;

                    // Read angle, speed, torque
                    params.angle = ReadFloatWithDefault("Angle value (degrees)", params.angle);
                    params.speed = ReadIntWithDefault("Speed value (0-100)", params.speed);
                    params.torque = ReadIntWithDefault("Torque value (0-100)", params.torque);

                    std::cout << "  ✓ Set successfully" << std::endl;

                } catch (const std::exception& e) {
                    std::cout << "  Input format error: " << e.what() << std::endl;
                }
            }

            // Build joint command list
            std::vector<JointCommand> joints;

            if (joint_params.empty()) {
                std::cout << "\nNo joints set, all joints will maintain current position" << std::endl;
            } else {
                for (const auto& pair : joint_params) {
                    int joint_id = pair.first;
                    const JointParams& params = pair.second;

                    // Use angle directly (degrees)
                    joints.push_back({
                        static_cast<JointId>(joint_id),
                        params.angle,
                        static_cast<int8_t>(params.speed),
                        static_cast<int8_t>(params.torque)
                    });
                }

                std::cout << "\nSending joint commands..." << std::endl;
            }

            // Send joint commands
            bool move_success = hand->MoveJoints(joints);

            if (move_success) {
                std::cout << "✓ Command sent successfully" << std::endl;

                // Wait for device to respond and get joint data
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                // Display current joint status (if callback data available)
                if (joints_received && !last_joints.empty()) {
                    std::cout << "\nCurrent Joint Status (partial display):" << std::endl;
                    std::cout << std::left << std::setw(20) << "Joint"
                              << std::setw(12) << "Angle(deg)"
                              << "State" << std::endl;
                    std::cout << std::string(40, '-') << std::endl;

                    // Only display first 5 joints as example
                    for (size_t i = 0; i < std::min(size_t(5), last_joints.size()); ++i) {
                        const Joint& joint = last_joints[i];
                        // GetJoints() returns angles in degrees, no conversion needed
                        std::cout << std::left << std::setw(20) << ToString(joint.id)
                                  << std::fixed << std::setprecision(1) << std::setw(12) << joint.angle
                                  << ToString(joint.state) << std::endl;
                    }
                    if (last_joints.size() > 5) {
                        std::cout << "... (and " << (last_joints.size() - 5) << " more joints)" << std::endl;
                    }
                }
            } else {
                std::cerr << "✗ Command send failed" << std::endl;
            }

            std::cout << "\n" << std::string(50, '=') << std::endl;
            std::cout << "Press Enter to start next control cycle..." << std::endl;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }

    } catch (const std::exception& e) {
        std::cout << "\nProgram exception: " << e.what() << std::endl;
    }

    // Disconnect
    std::cout << "\nDisconnecting..." << std::endl;
    hand->Disconnect();
    std::cout << "✓ Disconnected" << std::endl;

    std::cout << "\nProgram ended. Thank you for using!" << std::endl;
    return 0;
}
