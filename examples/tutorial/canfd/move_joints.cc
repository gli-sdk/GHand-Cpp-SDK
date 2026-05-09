#include "xiaoyao/xiaoyao.h"
#include "xiaoyao/logging.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace xiaoyao;

// 关节状态显示回调函数
void DisplayJoints(const std::vector<Joint>& joints) {
    std::cout << "\n========== Joint Status ==========" << std::endl;
    std::cout << std::left << std::setw(20) << "Joint"
              << std::setw(12) << "Angle(deg)"
              << std::setw(12) << "Velocity(%)"
              << std::setw(12) << "Torque(%)"
              << std::setw(15) << "State"
              << "Error" << std::endl;
    std::cout << std::string(90, '-') << std::endl;

    for (const auto& joint : joints) {
        std::cout << std::left << std::setw(20) << ToString(joint.id)
                  << std::fixed << std::setprecision(1) << std::setw(12) << joint.angle
                  << std::setw(12) << +joint.velocity
                  << std::setw(12) << +joint.torque
                  << std::setw(15) << ToString(joint.state)
                  << ToString(joint.error) << std::endl;
    }
    std::cout << "==================================" << std::endl;
}

int main() {
    xiaoyao::ConfigureConsole(xiaoyao::LogLevel::INFO);

    DexHand hand(ProductType::GHAND, CommType::CANFD);

    // 1. 搜索可用的 CANFD 适配器
    std::cout << "Searching for CANFD adapters..." << std::endl;
    std::map<std::string, std::string> adapters = hand.SearchAdapters();
    if (adapters.empty()) {
        std::cout << "No CANFD adapters found." << std::endl;
        return -1;
    }

    std::cout << "Found " << adapters.size() << " adapter(s):" << std::endl;
    for (const auto& adapter : adapters) {
        std::cout << "  " << adapter.first << " -> " << adapter.second << std::endl;
    }

    bool success = hand.Connect("auto");
    if (!success) {
        std::cout << "Failed to connect to CANFD device!" << std::endl;
        return -1;
    }

    std::cout << "Successfully connected via CANFD!" << std::endl;

    //// 4. 注册关节状态回调（CANFD 主动上报）
    hand.SetJointsCallback(DisplayJoints);
    std::cout << "Joint state callback registered (receiving active reports)." << std::endl;

    // 5. 设置控制模式并运动关节
    hand.SetControlMode(ControlMode::POSITION);

    std::vector<JointCommand> joints = {
        {JointId::THUMB_MCP, 30.0f, 100, 100},
        {JointId::THUMB_PIP, 45.0f, 100, 100},
    };

    std::cout << "\nMoving joints (FF_MCP=30, FF_PIP=45)..." << std::endl;
    if (hand.MoveJoints(joints)) {
        std::cout << "Joints moved successfully!" << std::endl;

        // 保持 5 秒，期间会打印主动上报的关节状态
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // 复位
        std::vector<JointCommand> reset = {
            {JointId::FF_MCP, 0.0f, 100, 100},
            {JointId::FF_PIP, 0.0f, 100, 100},
        };
        std::cout << "Resetting joints..." << std::endl;
        hand.MoveJoints(reset);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    } else {
        std::cout << "Failed to move joints!" << std::endl;
    }

    // 6. 断开连接
    hand.Disconnect();
    std::cout << "\nConnection closed." << std::endl;

    return 0;
}
