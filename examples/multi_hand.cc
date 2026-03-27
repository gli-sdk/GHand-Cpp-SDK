#include "xiaoyao/xiaoyao.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <map>

using namespace xiaoyao;

int main() {
    std::cout << "========================================\n";
    std::cout << "  枭尧灵巧手 SDK - 多手控制演示\n";
    std::cout << "========================================\n";

    // 搜索网络适配器
    DexHand temp_hand;
    std::map<std::string, std::string> adapters = temp_hand.SearchAdapters();

    if (adapters.empty()) {
        std::cerr << "未找到可用的网络适配器\n";
        return 1;
    }

    std::cout << "\n找到 " << adapters.size() << " 个可用适配器:\n";
    for (const auto& adapter : adapters) {
        const std::string& name = adapter.first;
        const std::string& desc = adapter.second;
        std::cout << "  - " << name << ": " << desc << '\n';
    }

    // 创建多个手部实例
    std::vector<std::unique_ptr<DexHand>> hands;

    std::cout << "\n尝试连接设备...\n";
    for (const auto& adapter : adapters) {
        const std::string& name = adapter.first;
        auto hand = std::make_unique<DexHand>();
        if (hand->Connect(CommType::ETHERCAT, name)) {
            hands.push_back(std::move(hand));
            std::cout << "  ✓ 已连接设备: " << name << '\n';
        } else {
            std::cout << "  ✗ 连接失败: " << name << '\n';
        }
    }

    if (hands.empty()) {
        std::cerr << "\n未能连接任何设备\n";
        return 1;
    }

    std::cout << "\n✓ 成功连接 " << hands.size() << " 个设备\n";

    // 显示所有设备信息
    std::cout << "\n设备信息:\n";
    for (size_t i = 0; i < hands.size(); ++i) {
        DeviceInfo info = hands[i]->GetDeviceInfo();
        std::cout << "  手部 " << i << ":\n";
        std::cout << "    设备名称: " << info.device_name << '\n';
        std::cout << "    硬件版本: " << info.hardware_version << '\n';
        std::cout << "    软件版本: " << info.software_version << '\n';
        std::cout << "    序列号: " << info.serial_number << '\n';
    }

    // 演示1：单独控制每个手
    std::cout << "\n========== 演示1：单独控制 ==========\n";

    std::vector<JointCommand> test_joints = {
        {JointId::FF_PIP, 45.0f, 100, 100},
        {JointId::FF_MCP, 30.0f, 100, 100},
    };

    for (size_t i = 0; i < hands.size(); ++i) {
        std::cout << "\n控制手部 " << i << "...\n";

        if (hands[i]->MoveJoints(test_joints)) {
            std::cout << "  ✓ 命令发送成功\n";
        } else {
            std::cout << "  ✗ 命令发送失败\n";
        }

        // 等待设备响应
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 保持姿势
    std::cout << "\n保持姿势2秒...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 演示2：同时控制所有手
    std::cout << "\n========== 演示2：同时控制 ==========\n";

    std::vector<JointCommand> reset_joints = {
        {JointId::THUMB_PIP, 0.0f, 100, 100},
        {JointId::THUMB_MCP, 0.0f, 100, 100},
        {JointId::THUMB_SWING, 0.0f, 100, 100},
        {JointId::THUMB_ROTATION, 0.0f, 100, 100},
        {JointId::FF_PIP, 0.0f, 100, 100},
        {JointId::FF_MCP, 0.0f, 100, 100},
        {JointId::FF_SWING, 0.0f, 100, 100},
        {JointId::MF_PIP, 0.0f, 100, 100},
        {JointId::MF_MCP, 0.0f, 100, 100},
        {JointId::RF_PIP, 0.0f, 100, 100},
        {JointId::RF_MCP, 0.0f, 100, 100},
        {JointId::LF_PIP, 0.0f, 100, 100},
        {JointId::LF_MCP, 0.0f, 100, 100},
    };

    std::cout << "\n同时复位所有手部...\n";
    for (auto& hand : hands) {
        hand->MoveJoints(reset_joints);
    }
    std::cout << "  ✓ 所有手部命令已发送\n";

    // 等待运动完成
    std::cout << "\n等待运动完成...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 断开所有连接
    std::cout << "\n========== 清理 ==========\n";
    std::cout << "\n断开所有连接...\n";
    for (size_t i = 0; i < hands.size(); ++i) {
        hands[i]->Disconnect();
        std::cout << "  ✓ 已关闭手部 " << i << '\n';
    }

    std::cout << "\n演示完成。感谢使用!\n";
    return 0;
}
