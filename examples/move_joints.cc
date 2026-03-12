#include "xiaoyao/dexhand.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using xiaoyao::JointId;

using namespace xiaoyao;

// 关节状态显示回调函数
void DisplayJoints(const std::vector<xiaoyao::Joint>& joints) {
    std::cout << "\n========== Joint Status ==========" << std::endl;
    std::cout << std::left << std::setw(20) << "Joint"
              << std::setw(10) << "Angle(°)"
              << std::setw(10) << "Velocity"
              << std::setw(10) << "Torque"
              << std::setw(15) << "State"
              << "Error" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto& joint : joints) {
        std::cout << std::left << std::setw(20) << xiaoyao::ToString(joint.id)
                  << std::fixed << std::setprecision(1) << std::setw(10) << joint.angle
                  << std::setw(10) << static_cast<int>(joint.velocity)
                  << std::setw(10) << static_cast<int>(joint.torque)
                  << std::setw(15) << xiaoyao::ToString(joint.state)
                  << xiaoyao::ToString(joint.error) << std::endl;
    }
    std::cout << "==================================" << std::endl;
}

int main() {
    xiaoyao::DexHand hand;

    // 尝试通过ETHERCAT连接灵巧手
    std::cout << "Connecting to dexterous hand via EtherCAT..." << std::endl;
    bool success = hand.AutoConnect(xiaoyao::CommType::ETHERCAT);

    if (success) {
        std::cout << "Successfully connected to the dexterous hand!" << std::endl;

        // 注册关节状态回调以实时显示关节数据
        hand.SetJointsCallback(DisplayJoints);
        std::cout << "Joint display callback registered." << std::endl;

        // 设置控制模式为位置模式（默认）
        hand.SetControlMode(xiaoyao::ControlMode::POSITION);

        // 定义关节命令：控制所有13个关节
        // DIP关节会被自动跳过，无需控制
        std::vector<xiaoyao::JointCommand> joints = {
            {JointId::FF_MCP, 30.0f, 100, 100},           // 食指掌指关节
            {JointId::FF_PIP, 45.0f, 100, 100},           // 食指近端指间关节
        };

        std::cout << "Moving joints..." << std::endl;
        bool move_success = hand.MoveJoints(joints);

        if (move_success) {
            std::cout << "Joints moved successfully!" << std::endl;

            // 保持姿势5秒
            std::this_thread::sleep_for(std::chrono::seconds(5));

            // 重置关节位置
            std::vector<xiaoyao::JointCommand> reset_joints = {
                {JointId::FF_MCP, 0.0f, 100, 100},
                {JointId::FF_PIP, 0.0f, 100, 100}
            };

            std::cout << "Resetting joint positions..." << std::endl;
            hand.MoveJoints(reset_joints);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } else {
            std::cout << "Failed to move joints!" << std::endl;
        }

        hand.Disconnect();
        std::cout << "Connection closed." << std::endl;
    } else {
        std::cout << "Failed to connect to the dexterous hand!" << std::endl;
    }

    return 0;
}
