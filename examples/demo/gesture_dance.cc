#include "ghand/ghand.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

using namespace ghand;

// ========== Gesture Pose Definitions ==========

// Thumb touches little finger
std::vector<JointCommand> MakeThumbTouchLittle() {
    return {
        {JointId::THUMB_PIP, 20.0f, 100, 100}, {JointId::THUMB_MCP, 50.0f, 100, 100},
        {JointId::THUMB_SWING, 60.0f, 100, 100}, {JointId::THUMB_ROTATION, 0.0f, 100, 100},
        {JointId::FF_PIP, 0.0f, 100, 100}, {JointId::FF_MCP, 0.0f, 100, 100}, {JointId::FF_SWING, 0.0f, 100, 100},
        {JointId::MF_PIP, 0.0f, 100, 100}, {JointId::MF_MCP, 0.0f, 100, 100},
        {JointId::RF_PIP, 0.0f, 100, 100}, {JointId::RF_MCP, 0.0f, 100, 100},
        {JointId::LF_PIP, 56.0f, 100, 100}, {JointId::LF_MCP, 28.0f, 100, 100},
    };
}

// Thumb touches ring finger
std::vector<JointCommand> MakeThumbTouchRing() {
    return {
        {JointId::THUMB_PIP, 19.0f, 100, 100}, {JointId::THUMB_MCP, 38.0f, 100, 100},
        {JointId::THUMB_SWING, 45.0f, 100, 100}, {JointId::THUMB_ROTATION, 0.0f, 100, 100},
        {JointId::FF_PIP, 0.0f, 100, 100}, {JointId::FF_MCP, 0.0f, 100, 100}, {JointId::FF_SWING, 0.0f, 100, 100},
        {JointId::MF_PIP, 0.0f, 100, 100}, {JointId::MF_MCP, 0.0f, 100, 100},
        {JointId::RF_PIP, 67.0f, 100, 100}, {JointId::RF_MCP, 39.0f, 100, 100},
        {JointId::LF_PIP, 0.0f, 100, 100}, {JointId::LF_MCP, 0.0f, 100, 100},
    };
}

// Thumb touches middle finger
std::vector<JointCommand> MakeThumbTouchMiddle() {
    return {
        {JointId::THUMB_PIP, 17.0f, 100, 100}, {JointId::THUMB_MCP, 27.0f, 100, 100},
        {JointId::THUMB_SWING, 30.0f, 100, 100}, {JointId::THUMB_ROTATION, 0.0f, 100, 100},
        {JointId::FF_PIP, 0.0f, 100, 100}, {JointId::FF_MCP, 0.0f, 100, 100}, {JointId::FF_SWING, 0.0f, 100, 100},
        {JointId::MF_PIP, 40.0f, 100, 100}, {JointId::MF_MCP, 57.0f, 100, 100},
        {JointId::RF_PIP, 0.0f, 100, 100}, {JointId::RF_MCP, 0.0f, 100, 100},
        {JointId::LF_PIP, 0.0f, 100, 100}, {JointId::LF_MCP, 0.0f, 100, 100},
    };
}

// Thumb touches index finger
std::vector<JointCommand> MakeThumbTouchIndex() {
    return {
        {JointId::THUMB_PIP, 13.0f, 100, 100}, {JointId::THUMB_MCP, 14.0f, 100, 100},
        {JointId::THUMB_SWING, 20.0f, 100, 100}, {JointId::THUMB_ROTATION, 0.0f, 100, 100},
        {JointId::FF_PIP, 51.0f, 100, 100}, {JointId::FF_MCP, 46.0f, 100, 100}, {JointId::FF_SWING, 0.0f, 100, 100},
        {JointId::MF_PIP, 0.0f, 100, 100}, {JointId::MF_MCP, 0.0f, 100, 100},
        {JointId::RF_PIP, 0.0f, 100, 100}, {JointId::RF_MCP, 0.0f, 100, 100},
        {JointId::LF_PIP, 0.0f, 100, 100}, {JointId::LF_MCP, 0.0f, 100, 100},
    };
}

// Fist pose
std::vector<JointCommand> MakeFist() {
    return {
        {JointId::THUMB_PIP, 40.0f, 100, 100}, {JointId::THUMB_MCP, 30.0f, 100, 100},
        {JointId::THUMB_SWING, 30.0f, 100, 100}, {JointId::THUMB_ROTATION, 0.0f, 100, 100},
        {JointId::FF_PIP, 65.0f, 100, 100}, {JointId::FF_MCP, 55.0f, 100, 100}, {JointId::FF_SWING, 0.0f, 100, 100},
        {JointId::MF_PIP, 65.0f, 100, 100}, {JointId::MF_MCP, 55.0f, 100, 100},
        {JointId::RF_PIP, 65.0f, 100, 100}, {JointId::RF_MCP, 55.0f, 100, 100},
        {JointId::LF_PIP, 65.0f, 100, 100}, {JointId::LF_MCP, 55.0f, 100, 100},
    };
}

// Open index finger
std::vector<JointCommand> MakeOpenIndexFinger() {
    return {
        {JointId::THUMB_PIP, 30.0f, 100, 100}, {JointId::THUMB_MCP, 20.0f, 100, 100},
        {JointId::THUMB_SWING, 20.0f, 100, 100}, {JointId::THUMB_ROTATION, 0.0f, 100, 100},
        {JointId::FF_PIP, 0.0f, 100, 100}, {JointId::FF_MCP, 0.0f, 100, 100}, {JointId::FF_SWING, 0.0f, 100, 100},
        {JointId::MF_PIP, 75.0f, 100, 100}, {JointId::MF_MCP, 70.0f, 100, 100},
        {JointId::RF_PIP, 75.0f, 100, 100}, {JointId::RF_MCP, 70.0f, 100, 100},
        {JointId::LF_PIP, 70.0f, 100, 100}, {JointId::LF_MCP, 70.0f, 100, 100},
    };
}

// Open middle finger
std::vector<JointCommand> MakeOpenMiddleFinger() {
    return {
        {JointId::THUMB_PIP, 30.0f, 100, 100}, {JointId::THUMB_MCP, 20.0f, 100, 100},
        {JointId::THUMB_SWING, 20.0f, 100, 100}, {JointId::THUMB_ROTATION, 0.0f, 100, 100},
        {JointId::FF_PIP, 0.0f, 100, 100}, {JointId::FF_MCP, 0.0f, 100, 100}, {JointId::FF_SWING, 0.0f, 100, 100},
        {JointId::MF_PIP, 0.0f, 100, 100}, {JointId::MF_MCP, 0.0f, 100, 100},
        {JointId::RF_PIP, 75.0f, 100, 100}, {JointId::RF_MCP, 70.0f, 100, 100},
        {JointId::LF_PIP, 70.0f, 100, 100}, {JointId::LF_MCP, 70.0f, 100, 100},
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

// OK pose
std::vector<JointCommand> MakeOK() {
    return {
        {JointId::THUMB_PIP, 40.0f, 100, 100}, {JointId::THUMB_MCP, 30.0f, 100, 100},
        {JointId::THUMB_SWING, 30.0f, 100, 100}, {JointId::THUMB_ROTATION, 0.0f, 100, 100},
        {JointId::FF_PIP, 30.0f, 100, 100}, {JointId::FF_MCP, 50.0f, 100, 100}, {JointId::FF_SWING, 0.0f, 100, 100},
        {JointId::MF_PIP, 0.0f, 100, 100}, {JointId::MF_MCP, 0.0f, 100, 100},
        {JointId::RF_PIP, 0.0f, 100, 100}, {JointId::RF_MCP, 0.0f, 100, 100},
        {JointId::LF_PIP, 0.0f, 100, 100}, {JointId::LF_MCP, 0.0f, 100, 100},
    };
}

// ========== Action Execution Functions ==========

bool HandZero(DexHand& hand) {
    auto joints = MakeOpenHand();
    return hand.MoveJoints(joints);
}

bool HandZero(DexHand& hand);

bool ThumbTouch(DexHand& hand) {
    const std::vector<std::vector<JointCommand>> poses = {
        MakeThumbTouchLittle(), MakeThumbTouchRing(),
        MakeThumbTouchMiddle(), MakeThumbTouchIndex()
    };
    for (const auto& pose : poses) {
        if (!hand.MoveJoints(pose)) return false;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return HandZero(hand);
}

bool FistThenOpen(DexHand& hand) {
    if (!hand.MoveJoints(MakeFist())) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return HandZero(hand);
}

bool SeqOpenFinger(DexHand& hand) {
    if (!hand.MoveJoints(MakeFist())) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (!hand.MoveJoints(MakeOpenIndexFinger())) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (!hand.MoveJoints(MakeOpenMiddleFinger())) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return HandZero(hand);
}

bool MakeOKGesture(DexHand& hand) {
    if (!hand.MoveJoints(MakeOK())) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return HandZero(hand);
}

// ========== First Action Group ==========
bool FirstAction(DexHand& hand) {
    if (!ThumbTouch(hand)) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for (int i = 0; i < 2; i++) {
        if (!FistThenOpen(hand)) return false;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return true;
}

// ========== Second Action Group ==========
bool SecondAction(DexHand& hand) {
    if (!SeqOpenFinger(hand)) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (!MakeOKGesture(hand)) return false;
    return true;
}

// ========== Main Function ==========
int main() {
    std::cout << "***** 枭尧灵巧手 SDK - 手势舞功能演示 *****\n" << std::endl;
    auto hand = DexHand::Create(ProductType::G5, CommType::ETHERCAT);
    if (!hand) {
        std::cerr << "Failed to create DexHand" << std::endl;
        return -1;
    }
    bool connected = hand->Connect("auto");
    if (!connected) {
        std::cout << "\n[Scan complete] Failed to connect to dexterous hand." << std::endl;
        return 1;
    }
    std::cout << "\n--- Device ready, starting gesture dance demo ---\n" << std::endl;

    int gesture_cycle = 0;
    const int max_cycles = 0;

    while (true) {
        gesture_cycle++;
        if (max_cycles > 0 && gesture_cycle > max_cycles) break;

        std::cout << "\n--- Round " << gesture_cycle << " gesture demo started ---" << std::endl;

        if (!FirstAction(*hand)) {
            std::cout << "第 " << gesture_cycle << " 轮演示中的第一组动作执行失败" << std::endl;
            break;
        }

        if (!SecondAction(*hand)) {
            std::cout << "第 " << gesture_cycle << " 轮演示中的第二组动作执行失败" << std::endl;
            break;
        }

        std::cout << "--- Round " << gesture_cycle << " gesture demo finished ---\n" << std::endl;

        if (max_cycles == 0) {
            std::cout << "Press Ctrl+C to stop demo and exit\n" << std::endl;
        }
    }

    hand->Disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "\n--- Demo finished, disconnecting ---" << std::endl;
    return 0;
}
