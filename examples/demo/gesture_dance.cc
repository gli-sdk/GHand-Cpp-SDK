#include "ghand/dexhand.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

using namespace ghand;

// ========== 手势姿态定义 ==========

// 拇指触碰小指
std::vector<JointCommand> MakeThumbTouchLittle() {
    return {
        {JointId::THUMB_PIP, 20.0f}, {JointId::THUMB_MCP, 50.0f},
        {JointId::THUMB_SWING, 60.0f}, {JointId::THUMB_ROTATION, 0.0f},
        {JointId::FF_PIP, 0.0f}, {JointId::FF_MCP, 0.0f}, {JointId::FF_SWING, 0.0f},
        {JointId::MF_PIP, 0.0f}, {JointId::MF_MCP, 0.0f},
        {JointId::RF_PIP, 0.0f}, {JointId::RF_MCP, 0.0f},
        {JointId::LF_PIP, 56.0f}, {JointId::LF_MCP, 28.0f},
    };
}

// 拇指触碰无名指
std::vector<JointCommand> MakeThumbTouchRing() {
    return {
        {JointId::THUMB_PIP, 19.0f}, {JointId::THUMB_MCP, 38.0f},
        {JointId::THUMB_SWING, 45.0f}, {JointId::THUMB_ROTATION, 0.0f},
        {JointId::FF_PIP, 0.0f}, {JointId::FF_MCP, 0.0f}, {JointId::FF_SWING, 0.0f},
        {JointId::MF_PIP, 0.0f}, {JointId::MF_MCP, 0.0f},
        {JointId::RF_PIP, 67.0f}, {JointId::RF_MCP, 39.0f},
        {JointId::LF_PIP, 0.0f}, {JointId::LF_MCP, 0.0f},
    };
}

// 拇指触碰中指
std::vector<JointCommand> MakeThumbTouchMiddle() {
    return {
        {JointId::THUMB_PIP, 17.0f}, {JointId::THUMB_MCP, 27.0f},
        {JointId::THUMB_SWING, 30.0f}, {JointId::THUMB_ROTATION, 0.0f},
        {JointId::FF_PIP, 0.0f}, {JointId::FF_MCP, 0.0f}, {JointId::FF_SWING, 0.0f},
        {JointId::MF_PIP, 40.0f}, {JointId::MF_MCP, 57.0f},
        {JointId::RF_PIP, 0.0f}, {JointId::RF_MCP, 0.0f},
        {JointId::LF_PIP, 0.0f}, {JointId::LF_MCP, 0.0f},
    };
}

// 拇指触碰食指
std::vector<JointCommand> MakeThumbTouchIndex() {
    return {
        {JointId::THUMB_PIP, 13.0f}, {JointId::THUMB_MCP, 14.0f},
        {JointId::THUMB_SWING, 20.0f}, {JointId::THUMB_ROTATION, 0.0f},
        {JointId::FF_PIP, 51.0f}, {JointId::FF_MCP, 46.0f}, {JointId::FF_SWING, 0.0f},
        {JointId::MF_PIP, 0.0f}, {JointId::MF_MCP, 0.0f},
        {JointId::RF_PIP, 0.0f}, {JointId::RF_MCP, 0.0f},
        {JointId::LF_PIP, 0.0f}, {JointId::LF_MCP, 0.0f},
    };
}

// 握拳姿态
std::vector<JointCommand> MakeFist() {
    return {
        {JointId::THUMB_PIP, 40.0f}, {JointId::THUMB_MCP, 30.0f},
        {JointId::THUMB_SWING, 30.0f}, {JointId::THUMB_ROTATION, 0.0f},
        {JointId::FF_PIP, 65.0f}, {JointId::FF_MCP, 55.0f}, {JointId::FF_SWING, 0.0f},
        {JointId::MF_PIP, 65.0f}, {JointId::MF_MCP, 55.0f},
        {JointId::RF_PIP, 65.0f}, {JointId::RF_MCP, 55.0f},
        {JointId::LF_PIP, 65.0f}, {JointId::LF_MCP, 55.0f},
    };
}

// 张开食指
std::vector<JointCommand> MakeOpenIndexFinger() {
    return {
        {JointId::THUMB_PIP, 30.0f}, {JointId::THUMB_MCP, 20.0f},
        {JointId::THUMB_SWING, 20.0f}, {JointId::THUMB_ROTATION, 0.0f},
        {JointId::FF_PIP, 0.0f}, {JointId::FF_MCP, 0.0f}, {JointId::FF_SWING, 0.0f},
        {JointId::MF_PIP, 75.0f}, {JointId::MF_MCP, 70.0f},
        {JointId::RF_PIP, 75.0f}, {JointId::RF_MCP, 70.0f},
        {JointId::LF_PIP, 70.0f}, {JointId::LF_MCP, 70.0f},
    };
}

// 张开中指
std::vector<JointCommand> MakeOpenMiddleFinger() {
    return {
        {JointId::THUMB_PIP, 30.0f}, {JointId::THUMB_MCP, 20.0f},
        {JointId::THUMB_SWING, 20.0f}, {JointId::THUMB_ROTATION, 0.0f},
        {JointId::FF_PIP, 0.0f}, {JointId::FF_MCP, 0.0f}, {JointId::FF_SWING, 0.0f},
        {JointId::MF_PIP, 0.0f}, {JointId::MF_MCP, 0.0f},
        {JointId::RF_PIP, 75.0f}, {JointId::RF_MCP, 70.0f},
        {JointId::LF_PIP, 70.0f}, {JointId::LF_MCP, 70.0f},
    };
}

// 张开手掌姿态
std::vector<JointCommand> MakeOpenHand() {
    return {
        {JointId::THUMB_PIP, 0.0f}, {JointId::THUMB_MCP, 0.0f},
        {JointId::THUMB_SWING, 20.0f}, {JointId::THUMB_ROTATION, 0.0f},
        {JointId::FF_PIP, 0.0f}, {JointId::FF_MCP, 0.0f}, {JointId::FF_SWING, 0.0f},
        {JointId::MF_PIP, 0.0f}, {JointId::MF_MCP, 0.0f},
        {JointId::RF_PIP, 0.0f}, {JointId::RF_MCP, 0.0f},
        {JointId::LF_PIP, 0.0f}, {JointId::LF_MCP, 0.0f},
    };
}

// OK姿态
std::vector<JointCommand> MakeOK() {
    return {
        {JointId::THUMB_PIP, 40.0f}, {JointId::THUMB_MCP, 30.0f},
        {JointId::THUMB_SWING, 30.0f}, {JointId::THUMB_ROTATION, 0.0f},
        {JointId::FF_PIP, 30.0f}, {JointId::FF_MCP, 50.0f}, {JointId::FF_SWING, 0.0f},
        {JointId::MF_PIP, 0.0f}, {JointId::MF_MCP, 0.0f},
        {JointId::RF_PIP, 0.0f}, {JointId::RF_MCP, 0.0f},
        {JointId::LF_PIP, 0.0f}, {JointId::LF_MCP, 0.0f},
    };
}

// ========== 动作执行函数 ==========

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

bool HandZero(DexHand& hand) {
    auto joints = MakeOpenHand();
    return hand.MoveJoints(joints);
}

// ========== 第一组动作 ==========
bool FirstAction(DexHand& hand) {
    if (!ThumbTouch(hand)) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for (int i = 0; i < 2; i++) {
        if (!FistThenOpen(hand)) return false;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return true;
}

// ========== 第二组动作 ==========
bool SecondAction(DexHand& hand) {
    if (!SeqOpenFinger(hand)) return false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (!MakeOKGesture(hand)) return false;
    return true;
}

// ========== 主函数 ==========
int main() {
    std::cout << "***** 枭尧灵巧手 SDK - 手势舞功能演示 *****\n" << std::endl;
    auto hand = DexHand::Create(ProductType::G5, CommType::ETHERCAT);
    if (!hand) {
        std::cerr << "Failed to create DexHand" << std::endl;
        return -1;
    }
    bool connected = hand->Connect("auto");
    if (!connected) {
        std::cout << "\n[扫描结束] 未能连接到灵巧手。" << std::endl;
        return 1;
    }
    std::cout << "\n--- 设备已就绪，将开始手势舞功能演示 ---\n" << std::endl;

    int gesture_cycle = 0;
    const int max_cycles = 0;

    while (true) {
        gesture_cycle++;
        if (max_cycles > 0 && gesture_cycle > max_cycles) break;

        std::cout << "\n--- 第 " << gesture_cycle << " 轮手势演示开始 ---" << std::endl;

        if (!FirstAction(*hand)) {
            std::cout << "第 " << gesture_cycle << " 轮演示中的第一组动作执行失败" << std::endl;
            break;
        }

        if (!SecondAction(*hand)) {
            std::cout << "第 " << gesture_cycle << " 轮演示中的第二组动作执行失败" << std::endl;
            break;
        }

        std::cout << "--- 第 " << gesture_cycle << " 轮手势演示结束 ---\n" << std::endl;

        if (max_cycles == 0) {
            std::cout << "按 Ctrl+C 停止演示并退出程序\n" << std::endl;
        }
    }

    hand->Disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "\n--- 演示结束，断开连接 ---" << std::endl;
    return 0;
}
