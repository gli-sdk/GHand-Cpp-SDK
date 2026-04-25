#include "xiaoyao/xiaoyao.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

using namespace xiaoyao;

// Pull pose
std::vector<JointCommand> MakePullPose() {
    return {
        {JointId::THUMB_PIP, 0.0f, 100, 100}, {JointId::THUMB_MCP, 0.0f, 100, 100},
        {JointId::THUMB_SWING, 20.0f, 100, 100}, {JointId::THUMB_ROTATION, 0.0f, 100, 100},
        {JointId::FF_PIP, 25.0f, 100, 100}, {JointId::FF_MCP, 35.0f, 100, 100}, {JointId::FF_SWING, 0.0f, 100, 100},
        {JointId::MF_PIP, 25.0f, 100, 100}, {JointId::MF_MCP, 35.0f, 100, 100},
        {JointId::RF_PIP, 25.0f, 100, 100}, {JointId::RF_MCP, 35.0f, 100, 100},
        {JointId::LF_PIP, 25.0f, 100, 100}, {JointId::LF_MCP, 35.0f, 100, 100},
    };
}

// Open hand pose
std::vector<JointCommand> MakeOpenHand() {
    return {
        {JointId::THUMB_PIP, 0.0f, 100, 100}, {JointId::THUMB_MCP, 0.0f, 100, 100},
        {JointId::THUMB_SWING, 20.0f, 100, 100}, {JointId::THUMB_ROTATION, 0.0f, 100, 100},
        {JointId::FF_PIP, 0.0f, 100, 100}, {JointId::FF_MCP, 0.0f, 100, 100}, {JointId::FF_SWING, 0.0f, 100, 100},
        {JointId::MF_PIP, 0.0f, 100, 100}, {JointId::MF_MCP, 0.0f, 100, 100},
        {JointId::RF_PIP, 0.0f, 100, 100}, {JointId::RF_MCP, 0.0f, 100, 100},
        {JointId::LF_PIP, 0.0f, 100, 100}, {JointId::LF_MCP, 0.0f, 100, 100},
    };
}

bool Pull(DexHand& hand) {
    auto joints = MakePullPose();
    return hand.MoveJoints(joints);
}

bool HandZero(DexHand& hand) {
    auto joints = MakeOpenHand();
    return hand.MoveJoints(joints);
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "***** Xiaoyao Dexterous Hand SDK - Pull Demo *****\n" << std::endl;
    DexHand hand;
    bool connected = hand.Connect(CommType::ETHERCAT, "auto");
    if (!connected) {
        std::cout << "\n[Scan complete] Failed to connect to dexterous hand." << std::endl;
        return 1;
    }
    std::cout << "\n--- Device ready, starting pull demo ---\n" << std::endl;

    int gesture_cycle = 0;
    const int max_cycles = 0;

    while (true) {
        gesture_cycle++;
        if (max_cycles > 0 && gesture_cycle > max_cycles) break;

        std::cout << "\n--- Round " << gesture_cycle << " demo started ---" << std::endl;

        if (!Pull(hand)) {
            std::cout << "Round " << gesture_cycle << " pull action execution failed" << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));

        if (!HandZero(hand)) {
            std::cout << "Round " << gesture_cycle << " reset action execution failed" << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::cout << "--- Round " << gesture_cycle << " demo finished ---\n" << std::endl;

        if (max_cycles == 0) {
            std::cout << "Press Ctrl+C to stop demo and exit\n" << std::endl;
        }
    }

    hand.Disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "\n--- Demo finished, disconnecting ---" << std::endl;
    return 0;
}
