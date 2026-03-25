#include "xiaoyao/xiaoyao.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <cmath>
#include <exception>

using namespace xiaoyao;

// ========== Gesture Type Definitions ==========

/**
 * @brief Preset gesture type enumeration
 */
enum class GestureType {
    OPEN_HAND,      // Open hand
    FIST,           // Fist
    OK,             // OK gesture
    THUMBS_UP,      // Thumbs up
    SIX_SIGN        // Six sign
};

/**
 * @brief Get English name of gesture
 * @param gesture Gesture type
 * @return English name of gesture
 */
std::string GetGestureName(GestureType gesture) {
    switch (gesture) {
        case GestureType::OPEN_HAND: return "Open Hand";
        case GestureType::FIST: return "Fist";
        case GestureType::OK: return "OK Gesture";
        case GestureType::THUMBS_UP: return "Thumbs Up";
        case GestureType::SIX_SIGN: return "Six Sign";
        default: return "Unknown Gesture";
    }
}

// ========== Gesture Definitions (joint angles in degrees) ==========

/**
 * @brief Gesture joint angle definitions
 *
 * Each gesture contains angle configurations for 13 controllable joints (in degrees)
 * Automatically converted to radians during execution
 */
const std::unordered_map<GestureType, std::unordered_map<JointId, float>> GESTURE_DEFINITIONS = {
    {
        GestureType::OPEN_HAND,
        {
            {JointId::THUMB_PIP, 0.0f},
            {JointId::THUMB_MCP, 0.0f},
            {JointId::THUMB_SWING, 0.0f},
            {JointId::THUMB_ROTATION, 0.0f},
            {JointId::FF_PIP, 0.0f},
            {JointId::FF_MCP, 0.0f},
            {JointId::FF_SWING, 0.0f},
            {JointId::MF_PIP, 0.0f},
            {JointId::MF_MCP, 0.0f},
            {JointId::RF_PIP, 0.0f},
            {JointId::RF_MCP, 0.0f},
            {JointId::LF_PIP, 0.0f},
            {JointId::LF_MCP, 0.0f},
        }
    },
    {
        GestureType::FIST,
        {
            {JointId::THUMB_PIP, 40.0f},
            {JointId::THUMB_MCP, 30.0f},
            {JointId::THUMB_SWING, 30.0f},
            {JointId::THUMB_ROTATION, 4.0f},
            {JointId::FF_PIP, 65.0f},
            {JointId::FF_MCP, 55.0f},
            {JointId::FF_SWING, 0.0f},
            {JointId::MF_PIP, 65.0f},
            {JointId::MF_MCP, 55.0f},
            {JointId::RF_PIP, 65.0f},
            {JointId::RF_MCP, 55.0f},
            {JointId::LF_PIP, 65.0f},
            {JointId::LF_MCP, 55.0f},
        }
    },
    {
        GestureType::OK,
        {
            {JointId::THUMB_PIP, 40.0f},
            {JointId::THUMB_MCP, 30.0f},
            {JointId::THUMB_SWING, 30.0f},
            {JointId::THUMB_ROTATION, 4.0f},
            {JointId::FF_PIP, 30.0f},
            {JointId::FF_MCP, 50.0f},
            {JointId::FF_SWING, 0.0f},
            {JointId::MF_PIP, 0.0f},
            {JointId::MF_MCP, 0.0f},
            {JointId::RF_PIP, 0.0f},
            {JointId::RF_MCP, 0.0f},
            {JointId::LF_PIP, 0.0f},
            {JointId::LF_MCP, 0.0f},
        }
    },
    {
        GestureType::THUMBS_UP,
        {
            {JointId::THUMB_PIP, 0.0f},
            {JointId::THUMB_MCP, 0.0f},
            {JointId::THUMB_SWING, 0.0f},
            {JointId::THUMB_ROTATION, 0.0f},
            {JointId::FF_PIP, 65.0f},
            {JointId::FF_MCP, 55.0f},
            {JointId::FF_SWING, 0.0f},
            {JointId::MF_PIP, 65.0f},
            {JointId::MF_MCP, 55.0f},
            {JointId::RF_PIP, 65.0f},
            {JointId::RF_MCP, 55.0f},
            {JointId::LF_PIP, 65.0f},
            {JointId::LF_MCP, 55.0f},
        }
    },
    {
        GestureType::SIX_SIGN,
        {
            {JointId::THUMB_PIP, 0.0f},
            {JointId::THUMB_MCP, 0.0f},
            {JointId::THUMB_SWING, 0.0f},
            {JointId::THUMB_ROTATION, 0.0f},
            {JointId::FF_PIP, 65.0f},
            {JointId::FF_MCP, 55.0f},
            {JointId::FF_SWING, 0.0f},
            {JointId::MF_PIP, 65.0f},
            {JointId::MF_MCP, 55.0f},
            {JointId::RF_PIP, 65.0f},
            {JointId::RF_MCP, 55.0f},
            {JointId::LF_PIP, 0.0f},
            {JointId::LF_MCP, 0.0f},
        }
    },
};

// ========== Helper Functions ==========

/**
 * @brief Convert gesture definition to joint command list
 * @param gesture_def Gesture definition (joint ID -> angle (degrees))
 * @param speed Speed percentage (0-100), default 100
 * @param torque Torque percentage (0-100), default 100
 * @return Joint command list
 */
std::vector<JointCommand> CreateJointsFromGesture(
    const std::unordered_map<JointId, float>& gesture_def,
    int8_t speed = 100,
    int8_t torque = 100
) {
    std::vector<JointCommand> joints;

    for (const auto& pair : gesture_def) {
        // Use angle directly (degrees), MoveJoints will convert to radians internally
        JointId joint_id = pair.first;
        float angle = pair.second;
        joints.push_back({joint_id, angle, speed, torque});
    }

    return joints;
}

/**
 * @brief Execute preset gesture
 * @param hand Dexterous hand instance
 * @param gesture Gesture type to execute
 * @param speed Speed percentage (0-100), default 100
 * @param torque Torque percentage (0-100), default 100
 * @return Returns true on success, false on failure
 */
bool ExecuteGesture(DexHand& hand,GestureType gesture,int8_t speed = 100,int8_t torque = 100) {
    // Find gesture definition
    auto it = GESTURE_DEFINITIONS.find(gesture);
    if (it == GESTURE_DEFINITIONS.end()) {
        std::cerr << "Error: Unknown gesture type" << std::endl;
        return false;
    }

    // Convert to joint commands and send
    const auto& gesture_def = it->second;
    std::vector<JointCommand> joints = CreateJointsFromGesture(gesture_def, speed, torque);

    return hand.MoveJoints(joints);
}

// ========== Main Program ==========

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Xiaoyao Dexterous Hand SDK - Preset Gesture Demo        " << std::endl;
    std::cout << "========================================" << std::endl;

    DexHand hand;

    // Connect device
    std::cout << "\nConnecting to dexterous hand via EtherCAT..." << std::endl;
    bool success = hand.AutoConnect(CommType::ETHERCAT);

    if (!success) {
        std::cerr << "Error: Unable to connect to dexterous hand!" << std::endl;
        return 1;
    }

    std::cout << "✓ Successfully connected to dexterous hand!" << std::endl;

    // Set control mode to position mode
    hand.SetControlMode(ControlMode::POSITION);

    // Define list of gestures to demonstrate
    const std::vector<GestureType> gesture_demo = {
        GestureType::OPEN_HAND,
        GestureType::FIST,
        GestureType::OK,
        GestureType::THUMBS_UP,
        GestureType::SIX_SIGN,
    };

    std::cout << "\nStarting preset gesture demonstration..." << std::endl;
    std::cout << "Press Ctrl+C to stop demonstration at any time\n" << std::endl;

    int cycle = 0;
    try {
        while (true) {
            cycle++;
            std::cout << "\n========== Cycle " << cycle << " ==========" << std::endl;

            // Demonstrate each gesture sequentially
            for (size_t i = 0; i < gesture_demo.size(); ++i) {
                GestureType gesture = gesture_demo[i];
                std::string gesture_name = GetGestureName(gesture);

                std::cout << "\n[" << (i + 1) << "/" << gesture_demo.size() << "] Executing gesture: " << gesture_name << std::endl;

                // Execute gesture
                if (!ExecuteGesture(hand, gesture, 100, 100)) {
                    std::cerr << "Error: Gesture execution failed!" << std::endl;
                    hand.Disconnect();
                    return 1;
                }

                // Wait for movement completion
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            }

            std::cout << "\n========== Cycle " << cycle << " completed ==========" << std::endl;
            std::cout << "Press Ctrl+C to stop demonstration, or continue to next cycle...\n" << std::endl;

            // Short delay before next cycle
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

    } catch (const std::exception& e) {
        std::cout << "\nProgram interrupted: " << e.what() << std::endl;
    }

    // Disconnect
    std::cout << "\nDisconnecting..." << std::endl;
    hand.Disconnect();
    std::cout << "✓ Disconnected" << std::endl;

    std::cout << "\nDemo completed. Thank you for using!" << std::endl;
    return 0;
}
