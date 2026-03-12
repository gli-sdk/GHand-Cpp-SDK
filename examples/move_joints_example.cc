#include "xiaoyao/dexhand.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    xiaoyao::DexHand hand;

    // 尝试通过ETHERCAT连接灵巧手
    std::cout << "Connecting to dexterous hand via EtherCAT..." << std::endl;
    bool success = hand.AutoConnect(xiaoyao::COMM_ETHERCAT);

    if (success) {
        std::cout << "Successfully connected to the dexterous hand!" << std::endl;

        // 获取手部类型
        xiaoyao::HandType type = hand.GetHandType();
        std::cout << "Hand type: " << xiaoyao::ToString(type) << std::endl;

        // 获取固件版本
        xiaoyao::DeviceInfo device_info = hand.GetDeviceInfo();
        std::string version = device_info.software_version;
        if (!version.empty()) {
            std::cout << "Firmware version: " << version << std::endl;
        }

        // 设置控制模式为位置模式（默认）
        hand.SetControlMode(xiaoyao::ControlMode::POSITION);

        // 定义关节命令：让大拇指弯曲30度
        std::vector<xiaoyao::JointCommand> commands;
        xiaoyao::JointCommand thumb_command;
        thumb_command.id = xiaoyao::THUMB_PIP;  // 大拇指近端指间关节
        thumb_command.angle = 30.0f;            // 目标角度（度）
        thumb_command.velocity = 50;            // 速度（0-100）
        thumb_command.torque = 30;              // 力矩（0-100）
        commands.push_back(thumb_command);

        // 让食指弯曲45度
        xiaoyao::JointCommand index_command;
        index_command.id = xiaoyao::FF_PIP;     // 食指近端指间关节
        index_command.angle = 45.0f;            // 目标角度（度）
        index_command.velocity = 50;            // 速度（0-100）
        index_command.torque = 30;              // 力矩（0-100）
        commands.push_back(index_command);

        std::cout << "Moving joints..." << std::endl;
        bool move_success = hand.MoveJoints(commands);

        if (move_success) {
            std::cout << "Joints moved successfully!" << std::endl;

            // 保持姿势5秒
            std::this_thread::sleep_for(std::chrono::seconds(5));

            // 重置关节位置
            std::vector<xiaoyao::JointCommand> reset_commands;
            xiaoyao::JointCommand thumb_reset;
            thumb_reset.id = xiaoyao::THUMB_PIP;
            thumb_reset.angle = 0.0f;
            thumb_reset.velocity = 50;
            thumb_reset.torque = 30;
            reset_commands.push_back(thumb_reset);

            xiaoyao::JointCommand index_reset;
            index_reset.id = xiaoyao::FF_PIP;
            index_reset.angle = 0.0f;
            index_reset.velocity = 50;
            index_reset.torque = 30;
            reset_commands.push_back(index_reset);

            std::cout << "Resetting joint positions..." << std::endl;
            hand.MoveJoints(reset_commands);
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
