#include "ghand/dexhand.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <sstream>

using namespace ghand;

// ========== Gesture Type Definitions ==========

enum class GestureType {
    OPEN_HAND,
    FIST,
    OK,
    THUMBS_UP,
    SIX_SIGN
};

std::string GetGestureName(GestureType gesture) {
    switch (gesture) {
        case GestureType::OPEN_HAND: return "Open Hand";
        case GestureType::FIST: return "Fist";
        case GestureType::OK: return "OK Gesture";
        case GestureType::THUMBS_UP: return "Thumbs Up";
        case GestureType::SIX_SIGN: return "Six Sign";
        default: return "Unknown";
    }
}

// ========== Gesture Definitions ==========

const std::unordered_map<GestureType, std::unordered_map<ghand::JointId, float>> GESTURE_DEFINITIONS = {
    {
        GestureType::OPEN_HAND,
        {
            {ghand::JointId::THUMB_PIP, 0.0f}, {ghand::JointId::THUMB_MCP, 0.0f},
            {ghand::JointId::THUMB_SWING, 0.0f}, {ghand::JointId::THUMB_ROTATION, 0.0f},
            {ghand::JointId::FF_PIP, 0.0f}, {ghand::JointId::FF_MCP, 0.0f},
            {ghand::JointId::FF_SWING, 0.0f}, {ghand::JointId::MF_PIP, 0.0f},
            {ghand::JointId::MF_MCP, 0.0f}, {ghand::JointId::RF_PIP, 0.0f},
            {ghand::JointId::RF_MCP, 0.0f}, {ghand::JointId::LF_PIP, 0.0f},
            {ghand::JointId::LF_MCP, 0.0f},
        }
    },
    {
        GestureType::FIST,
        {
            {ghand::JointId::THUMB_PIP, 40.0f}, {ghand::JointId::THUMB_MCP, 30.0f},
            {ghand::JointId::THUMB_SWING, 30.0f}, {ghand::JointId::THUMB_ROTATION, 4.0f},
            {ghand::JointId::FF_PIP, 65.0f}, {ghand::JointId::FF_MCP, 55.0f},
            {ghand::JointId::FF_SWING, 0.0f}, {ghand::JointId::MF_PIP, 65.0f},
            {ghand::JointId::MF_MCP, 55.0f}, {ghand::JointId::RF_PIP, 65.0f},
            {ghand::JointId::RF_MCP, 55.0f}, {ghand::JointId::LF_PIP, 65.0f},
            {ghand::JointId::LF_MCP, 55.0f},
        }
    },
    {
        GestureType::OK,
        {
            {ghand::JointId::THUMB_PIP, 40.0f}, {ghand::JointId::THUMB_MCP, 30.0f},
            {ghand::JointId::THUMB_SWING, 30.0f}, {ghand::JointId::THUMB_ROTATION, 4.0f},
            {ghand::JointId::FF_PIP, 30.0f}, {ghand::JointId::FF_MCP, 50.0f},
            {ghand::JointId::FF_SWING, 0.0f}, {ghand::JointId::MF_PIP, 0.0f},
            {ghand::JointId::MF_MCP, 0.0f}, {ghand::JointId::RF_PIP, 0.0f},
            {ghand::JointId::RF_MCP, 0.0f}, {ghand::JointId::LF_PIP, 0.0f},
            {ghand::JointId::LF_MCP, 0.0f},
        }
    },
    {
        GestureType::THUMBS_UP,
        {
            {ghand::JointId::THUMB_PIP, 0.0f}, {ghand::JointId::THUMB_MCP, 0.0f},
            {ghand::JointId::THUMB_SWING, 0.0f}, {ghand::JointId::THUMB_ROTATION, 0.0f},
            {ghand::JointId::FF_PIP, 65.0f}, {ghand::JointId::FF_MCP, 55.0f},
            {ghand::JointId::FF_SWING, 0.0f}, {ghand::JointId::MF_PIP, 65.0f},
            {ghand::JointId::MF_MCP, 55.0f}, {ghand::JointId::RF_PIP, 65.0f},
            {ghand::JointId::RF_MCP, 55.0f}, {ghand::JointId::LF_PIP, 65.0f},
            {ghand::JointId::LF_MCP, 55.0f},
        }
    },
    {
        GestureType::SIX_SIGN,
        {
            {ghand::JointId::THUMB_PIP, 0.0f}, {ghand::JointId::THUMB_MCP, 0.0f},
            {ghand::JointId::THUMB_SWING, 0.0f}, {ghand::JointId::THUMB_ROTATION, 0.0f},
            {ghand::JointId::FF_PIP, 65.0f}, {ghand::JointId::FF_MCP, 55.0f},
            {ghand::JointId::FF_SWING, 0.0f}, {ghand::JointId::MF_PIP, 65.0f},
            {ghand::JointId::MF_MCP, 55.0f}, {ghand::JointId::RF_PIP, 65.0f},
            {ghand::JointId::RF_MCP, 55.0f}, {ghand::JointId::LF_PIP, 0.0f},
            {ghand::JointId::LF_MCP, 0.0f},
        }
    },
};

// ========== Global State for Error Handling ==========

ghand::HandState g_hand_state;
std::vector<ghand::Joint> g_joints;
std::mutex g_state_mutex;

void OnHandStateUpdate(const ghand::HandState& state) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    g_hand_state = state;
}

void OnJointsUpdate(const std::vector<ghand::Joint>& joints) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    g_joints = joints;
}

bool HasError() {
    std::lock_guard<std::mutex> lock(g_state_mutex);

    // Check hand state first (priority 1)
    if (g_hand_state.HasError()) {
        return true;
    }

    // Check joints (priority 2)
    for (const auto& joint : g_joints) {
        if (joint.HasError()) {
            return true;
        }
    }

    return false;
}

void PrintError() {
    std::lock_guard<std::mutex> lock(g_state_mutex);

    // Print hand state errors
    if (g_hand_state.HasError()) {
        std::cerr << "\n[ERROR] Hand state error detected!" << std::endl;
        std::cerr << "Error: " << ghand::ToString(g_hand_state.error) << std::endl;
        std::cerr << "State: " << ghand::ToString(g_hand_state.state) << std::endl;
        std::cerr << "Temperature: " << g_hand_state.temperature << " degC" << std::endl;
    }

    // Print joint errors
    std::vector<std::string> faulty_joints;
    for (const auto& joint : g_joints) {
        if (joint.HasError()) {
            std::stringstream ss;
            ss << "  - " << ghand::ToString(joint.id)
               << ": state=" << ghand::ToString(joint.state)
               << ", error=" << ghand::ToString(joint.error);
            faulty_joints.push_back(ss.str());
        }
    }

    if (!faulty_joints.empty()) {
        if (g_hand_state.HasError()) {
            std::cerr << std::endl;
        }
        std::cerr << "[ERROR] Detected " << faulty_joints.size() << " faulty joint(s)" << std::endl;
        std::cerr << "Faulty joints:" << std::endl;
        for (const auto& fault : faulty_joints) {
            std::cerr << fault << std::endl;
        }
    }
}

// ========== Helper Functions ==========

std::vector<ghand::JointCommand> CreateJointsFromGesture(
    const std::unordered_map<ghand::JointId, float>& gesture_def) {
    std::vector<ghand::JointCommand> joints;
    for (const auto& pair : gesture_def) {
        joints.push_back({pair.first, pair.second, 100, 100});
    }
    return joints;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  GHand Dexterous Hand SDK - Preset Gesture Demo" << std::endl;
    std::cout << "========================================" << std::endl;

    // Connect to device
    auto hand = ghand::DexHand::Create(ghand::ProductType::G5, ghand::CommType::ETHERCAT);
    if (!hand) {
        std::cerr << "Failed to create DexHand" << std::endl;
        return -1;
    }
    std::cout << "\nConnecting to dexterous hand..." << std::endl;
    if (!hand->AutoConnect()) {
        std::cerr << "Failed to connect!" << std::endl;
        return 1;
    }
    std::cout << "Connected successfully" << std::endl;

    // Set control mode
    hand->SetControlMode(ghand::ControlMode::POSITION);

    // Register callbacks for error detection
    hand->SetJointsCallback(OnJointsUpdate);
    hand->SetHandStateCallback(OnHandStateUpdate);

    // Gesture sequence
    const std::vector<GestureType> gestures = {
        GestureType::OPEN_HAND,
        GestureType::FIST,
        GestureType::OK,
        GestureType::THUMBS_UP,
        GestureType::SIX_SIGN,
    };

    // Run demo cycles
    int cycle = 0;
    const int kGestureWaitMs = 1500;
    const int kCycleDelayMs = 500;
    const int kErrorCheckIntervalMs = 100;

    std::cout << "\nStarting gesture demonstration..." << std::endl;
    std::cout << "Press Ctrl+C to stop\n" << std::endl;

    bool has_error = false;
    while (!has_error) {
        cycle++;
        std::cout << "\n========== Cycle " << cycle << " ==========" << std::endl;

        // Execute each gesture
        for (auto gesture : gestures) {
            // Check for errors before executing gesture
            if (HasError()) {
                has_error = true;
                break;
            }

            std::cout << "\nExecuting: " << GetGestureName(gesture) << std::endl;

            // Get gesture definition and execute
            auto it = GESTURE_DEFINITIONS.find(gesture);
            if (it != GESTURE_DEFINITIONS.end()) {
                auto joints = CreateJointsFromGesture(it->second);
                hand->MoveJoints(joints);
            }

            // Wait for completion with error checking
            int elapsed = 0;
            while (elapsed < kGestureWaitMs && !has_error) {
                std::this_thread::sleep_for(std::chrono::milliseconds(kErrorCheckIntervalMs));
                elapsed += kErrorCheckIntervalMs;

                if (HasError()) {
                    has_error = true;
                    break;
                }
            }

            if (has_error) break;
        }

        if (has_error) break;

        std::cout << "\n========== Cycle " << cycle << " completed ==========" << std::endl;
        std::cout << "Press Ctrl+C to stop, or continue to next cycle...\n" << std::endl;

        // Wait between cycles with error checking
        int elapsed = 0;
        while (elapsed < kCycleDelayMs && !has_error) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kErrorCheckIntervalMs));
            elapsed += kErrorCheckIntervalMs;

            if (HasError()) {
                has_error = true;
                break;
            }
        }
    }

    // Handle error if detected
    if (has_error) {
        PrintError();
        std::cerr << "\nStopping all motion and clearing fault..." << std::endl;
        hand->Stop();
        hand->ClearFault();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Cleanup
    std::cout << "\nDisconnecting..." << std::endl;
    hand->Disconnect();
    std::cout << "Disconnected. Thank you!" << std::endl;

    return has_error ? 1 : 0;
}
