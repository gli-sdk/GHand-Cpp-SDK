#include "ghand/dexhand.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

using namespace xiaoyao;

// 敲击姿态
std::vector<JointCommand> MakeKnockPose() {
    return {
        {JointId::THUMB_PIP, 60.0f}, {JointId::THUMB_MCP, 0.0f},
        {JointId::THUMB_SWING, 20.0f}, {JointId::THUMB_ROTATION, 0.0f},
        {JointId::FF_PIP, 75.0f}, {JointId::FF_MCP, 0.0f}, {JointId::FF_SWING, 0.0f},
        {JointId::MF_PIP, 75.0f}, {JointId::MF_MCP, 0.0f},
        {JointId::RF_PIP, 75.0f}, {JointId::RF_MCP, 70.0f},
        {JointId::LF_PIP, 74.0f}, {JointId::LF_MCP, 70.0f},
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

bool Knock(DexHand& hand) {
    auto joints = MakeKnockPose();
    return hand.MoveJoints(joints);
}

bool HandZero(DexHand& hand) {
    auto joints = MakeOpenHand();
    return hand.MoveJoints(joints);
}

int main() {
    std::cout << "***** 枭尧灵巧手 SDK - 敲功能演示 *****\n" << std::endl;
    auto hand = DexHand::Create(ProductType::GHAND, CommType::ETHERCAT);
    if (!hand) {
        std::cerr << "Failed to create DexHand" << std::endl;
        return -1;
    }
    bool connected = hand->Connect("auto");
    if (!connected) {
        std::cout << "\n[扫描结束] 未能连接到灵巧手。" << std::endl;
        return 1;
    }
    std::cout << "\n--- 设备已就绪，将开始敲功能演示 ---\n" << std::endl;

    int gesture_cycle = 0;
    const int max_cycles = 0;

    while (true) {
        gesture_cycle++;
        if (max_cycles > 0 && gesture_cycle > max_cycles) break;

        std::cout << "\n--- 第 " << gesture_cycle << " 轮功能演示开始 ---" << std::endl;

        if (!Knock(*hand)) {
            std::cout << "第 " << gesture_cycle << " 轮演示中的敲击动作执行失败" << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));

        if (!HandZero(*hand)) {
            std::cout << "第 " << gesture_cycle << " 轮演示中的复位动作执行失败" << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::cout << "--- 第 " << gesture_cycle << " 轮功能演示结束 ---\n" << std::endl;

        if (max_cycles == 0) {
            std::cout << "按 Ctrl+C 停止演示并退出程序\n" << std::endl;
        }
    }

    hand->Disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "\n--- 演示结束，断开连接 ---" << std::endl;
    return 0;
}
