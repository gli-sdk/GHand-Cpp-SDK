#include "xiaoyao/xiaoyao.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

using namespace xiaoyao;

/**
 * @brief Speed control demonstration
 *
 * This example shows how to control dexterous hand joint movements using different speed percentages.
 * Speed parameter range: 0-100%, higher values result in faster movement.
 */

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Xiaoyao Dexterous Hand SDK - Speed Control Demo        " << std::endl;
    std::cout << "========================================" << std::endl;

    DexHand hand(CommType::ETHERCAT);

    // Connect device
    std::cout << "\nConnecting to dexterous hand via EtherCAT..." << std::endl;
    bool success = hand.AutoConnect();

    if (!success) {
        std::cerr << "Error: Unable to connect to dexterous hand!" << std::endl;
        return 1;
    }

    std::cout << "✓ Successfully connected to dexterous hand!" << std::endl;

    // Set control mode to position mode
    hand.SetControlMode(ControlMode::POSITION);

    // ========== Demo 1: Fist movement with different speeds ==========
    std::cout << "\n========== Demo 1: Fist movement with different speeds ==========" << std::endl;
    std::cout << "Demonstrating fist movements at 25%, 50%, 75%, 100% speeds" << std::endl;
    std::cout << "Observe the speed differences of finger movements at different speeds\n" << std::endl;

    // Define fist gesture
    std::vector<JointCommand> fist_joints = {
        {JointId::THUMB_PIP, 40.0f, 0, 50},
        {JointId::THUMB_MCP, 30.0f, 0, 50},
        {JointId::THUMB_SWING, 30.0f, 0, 50},
        {JointId::THUMB_ROTATION, 4.0f, 0, 50},
        {JointId::FF_PIP, 65.0f, 0, 50},
        {JointId::FF_MCP, 55.0f, 0, 50},
        {JointId::MF_PIP, 65.0f, 0, 50},
        {JointId::MF_MCP, 55.0f, 0, 50},
        {JointId::RF_PIP, 65.0f, 0, 50},
        {JointId::RF_MCP, 55.0f, 0, 50},
        {JointId::LF_PIP, 65.0f, 0, 50},
        {JointId::LF_MCP, 55.0f, 0, 50}
    };

    // Define open hand gesture
    std::vector<JointCommand> open_joints = {
        {JointId::THUMB_PIP, 0.0f, 0, 50},
        {JointId::THUMB_MCP, 0.0f, 0, 50},
        {JointId::THUMB_SWING, 0.0f, 0, 50},
        {JointId::THUMB_ROTATION, 0.0f, 0, 50},
        {JointId::FF_PIP, 0.0f, 0, 50},
        {JointId::FF_MCP, 0.0f, 0, 50},
        {JointId::MF_PIP, 0.0f, 0, 50},
        {JointId::MF_MCP, 0.0f, 0, 50},
        {JointId::RF_PIP, 0.0f, 0, 50},
        {JointId::RF_MCP, 0.0f, 0, 50},
        {JointId::LF_PIP, 0.0f, 0, 50},
        {JointId::LF_MCP, 0.0f, 0, 50}
    };

    // Test different speed values
    std::vector<uint8_t> speed_levels = {25, 50, 75, 100};

    for (uint8_t speed : speed_levels) {
        std::cout << ">>> Executing fist, speed: " << static_cast<int>(speed) << "%" << std::endl;

        // Set speed and execute fist
        for (auto& joint : fist_joints) {
            joint.velocity = speed;
        }
        hand.MoveJoints(fist_joints);

        // Wait for movement completion (faster speed = shorter wait time)
        int wait_time = 1500 + (100 - speed) * 10;
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));

        std::cout << ">>> Opening hand, speed: " << static_cast<int>(speed) << "%" << std::endl;

        // Set speed and open hand
        for (auto& joint : open_joints) {
            joint.velocity = speed;
        }
        hand.MoveJoints(open_joints);

        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        std::cout << std::endl;
    }

    // ========== Demo 2: Gradual speed control ==========
    std::cout << "\n========== Demo 2: Gradual speed control ==========" << std::endl;
    std::cout << "Gradually increasing from 10% to 100%, observe speed changes\n" << std::endl;

    for (int i = 1; i <= 10; i++) {
        uint8_t speed = i * 10;
        std::cout << ">>> Speed: " << static_cast<int>(speed) << "%" << std::endl;

        // Set speed
        for (auto& joint : fist_joints) {
            joint.velocity = speed;
        }
        hand.MoveJoints(fist_joints);

        // Wait
        std::this_thread::sleep_for(std::chrono::milliseconds(800));

        // Open hand
        for (auto& joint : open_joints) {
            joint.velocity = speed;
        }
        hand.MoveJoints(open_joints);

        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    // ========== Demo 3: Different speeds for different fingers ==========
    std::cout << "\n========== Demo 3: Different speeds for different fingers ==========" << std::endl;
    std::cout << "Setting different speeds for each finger to create wave effect\n" << std::endl;

    // Create joint commands with each finger moving at different speeds
    std::vector<JointCommand> wave_fist = {
        // Thumb - fastest
        {JointId::THUMB_PIP, 40.0f, 100, 50},
        {JointId::THUMB_MCP, 30.0f, 100, 50},
        {JointId::THUMB_SWING, 30.0f, 100, 50},
        {JointId::THUMB_ROTATION, 4.0f, 100, 50},
        // Index finger - fast
        {JointId::FF_PIP, 65.0f, 80, 50},
        {JointId::FF_MCP, 55.0f, 80, 50},
        // Middle finger - medium
        {JointId::MF_PIP, 65.0f, 60, 50},
        {JointId::MF_MCP, 55.0f, 60, 50},
        // Ring finger - slow
        {JointId::RF_PIP, 65.0f, 40, 50},
        {JointId::RF_MCP, 55.0f, 40, 50},
        // Little finger - slowest
        {JointId::LF_PIP, 65.0f, 20, 50},
        {JointId::LF_MCP, 55.0f, 20, 50}
    };

    std::cout << ">>> Executing wave fist (thumb fastest, little finger slowest)" << std::endl;
    hand.MoveJoints(wave_fist);
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    std::cout << ">>> Opening hand" << std::endl;
    hand.MoveJoints(open_joints);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // ========== Demo completed ==========
    std::cout << "\n========== Speed control demo completed ==========" << std::endl;
    std::cout << "Key points:" << std::endl;
    std::cout << "1. Speed parameter range: 0-100%" << std::endl;
    std::cout << "2. Higher speed values result in faster joint movement" << std::endl;
    std::cout << "3. Each joint can be set with individual speed" << std::endl;
    std::cout << "4. Different speed combinations can create complex motion effects" << std::endl;

    // Disconnect
    std::cout << "\nDisconnecting..." << std::endl;
    hand.Disconnect();
    std::cout << "✓ Disconnected" << std::endl;

    std::cout << "\nDemo completed. Thank you for using!" << std::endl;
    return 0;
}
