#include "xiaoyao/xiaoyao.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <cmath>
#include <exception>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace xiaoyao;

// ========== 手势类型定义 ==========

/**
 * @brief 预设手势类型枚举
 */
enum class GestureType {
    OPEN_HAND,      // 张开手
    FIST,           // 握拳
    OK,             // OK手势
    THUMBS_UP,      // 竖大拇指
    SIX_SIGN        // 六手势
};

/**
 * @brief 获取手势的中文名称
 * @param gesture 手势类型
 * @return 手势的中文名称
 */
std::string GetGestureName(GestureType gesture) {
    switch (gesture) {
        case GestureType::OPEN_HAND: return "张开手";
        case GestureType::FIST: return "握拳";
        case GestureType::OK: return "OK手势";
        case GestureType::THUMBS_UP: return "竖大拇指";
        case GestureType::SIX_SIGN: return "六手势";
        default: return "未知手势";
    }
}

// ========== 手势定义（关节角度，单位：度）==========

/**
 * @brief 手势关节角度定义
 *
 * 每个手势包含13个可控关节的角度配置（单位：度）
 * 执行时会自动转换为弧度
 */
const std::unordered_map<GestureType, std::unordered_map<JointId, float>> GESTURE_DEFINITIONS = {
    {
        GestureType::OPEN_HAND,
        {
            {JointId::THUMB_PIP, 0.0f},
            {JointId::THUMB_MCP, 0.0f},
            {JointId::THUMB_SWING, 0.0f},
            {JointId::THUMB_ROTATION, 0.0f},
            {JointId::FF_PIP, 0.0f},
            {JointId::FF_MCP, 0.0f},
            {JointId::FF_SWING, 0.0f},
            {JointId::MF_PIP, 0.0f},
            {JointId::MF_MCP, 0.0f},
            {JointId::RF_PIP, 0.0f},
            {JointId::RF_MCP, 0.0f},
            {JointId::LF_PIP, 0.0f},
            {JointId::LF_MCP, 0.0f},
        }
    },
    {
        GestureType::FIST,
        {
            {JointId::THUMB_PIP, 40.0f},
            {JointId::THUMB_MCP, 30.0f},
            {JointId::THUMB_SWING, 30.0f},
            {JointId::THUMB_ROTATION, 4.0f},
            {JointId::FF_PIP, 65.0f},
            {JointId::FF_MCP, 55.0f},
            {JointId::FF_SWING, 0.0f},
            {JointId::MF_PIP, 65.0f},
            {JointId::MF_MCP, 55.0f},
            {JointId::RF_PIP, 65.0f},
            {JointId::RF_MCP, 55.0f},
            {JointId::LF_PIP, 65.0f},
            {JointId::LF_MCP, 55.0f},
        }
    },
    {
        GestureType::OK,
        {
            {JointId::THUMB_PIP, 40.0f},
            {JointId::THUMB_MCP, 30.0f},
            {JointId::THUMB_SWING, 30.0f},
            {JointId::THUMB_ROTATION, 4.0f},
            {JointId::FF_PIP, 30.0f},
            {JointId::FF_MCP, 50.0f},
            {JointId::FF_SWING, 0.0f},
            {JointId::MF_PIP, 0.0f},
            {JointId::MF_MCP, 0.0f},
            {JointId::RF_PIP, 0.0f},
            {JointId::RF_MCP, 0.0f},
            {JointId::LF_PIP, 0.0f},
            {JointId::LF_MCP, 0.0f},
        }
    },
    {
        GestureType::THUMBS_UP,
        {
            {JointId::THUMB_PIP, 0.0f},
            {JointId::THUMB_MCP, 0.0f},
            {JointId::THUMB_SWING, 0.0f},
            {JointId::THUMB_ROTATION, 0.0f},
            {JointId::FF_PIP, 65.0f},
            {JointId::FF_MCP, 55.0f},
            {JointId::FF_SWING, 0.0f},
            {JointId::MF_PIP, 65.0f},
            {JointId::MF_MCP, 55.0f},
            {JointId::RF_PIP, 65.0f},
            {JointId::RF_MCP, 55.0f},
            {JointId::LF_PIP, 65.0f},
            {JointId::LF_MCP, 55.0f},
        }
    },
    {
        GestureType::SIX_SIGN,
        {
            {JointId::THUMB_PIP, 0.0f},
            {JointId::THUMB_MCP, 0.0f},
            {JointId::THUMB_SWING, 0.0f},
            {JointId::THUMB_ROTATION, 0.0f},
            {JointId::FF_PIP, 65.0f},
            {JointId::FF_MCP, 55.0f},
            {JointId::FF_SWING, 0.0f},
            {JointId::MF_PIP, 65.0f},
            {JointId::MF_MCP, 55.0f},
            {JointId::RF_PIP, 65.0f},
            {JointId::RF_MCP, 55.0f},
            {JointId::LF_PIP, 0.0f},
            {JointId::LF_MCP, 0.0f},
        }
    },
};

// ========== 辅助函数 ==========

/**
 * @brief 将手势定义转换为关节命令列表
 * @param gesture_def 手势定义（关节ID -> 角度（度））
 * @param speed 速度百分比（0-100），默认100
 * @param torque 力矩百分比（0-100），默认100
 * @return 关节命令列表
 */
std::vector<JointCommand> CreateJointsFromGesture(
    const std::unordered_map<JointId, float>& gesture_def,
    uint8_t speed = 100,
    uint8_t torque = 100
) {
    std::vector<JointCommand> joints;

    for (const auto& pair : gesture_def) {
        // 将角度从度转换为弧度
        JointId joint_id = pair.first;
        float angle_deg = pair.second;
        float angle_rad = angle_deg * M_PI / 180.0f;
        joints.push_back({joint_id, angle_rad, speed, torque});
    }

    return joints;
}

/**
 * @brief 执行预设手势
 * @param hand 灵巧手实例
 * @param gesture 要执行的手势类型
 * @param speed 速度百分比（0-100），默认100
 * @param torque 力矩百分比（0-100），默认100
 * @return 成功返回true，失败返回false
 */
bool ExecuteGesture(DexHand& hand,GestureType gesture,uint8_t speed = 100,uint8_t torque = 100) {
    // 查找手势定义
    auto it = GESTURE_DEFINITIONS.find(gesture);
    if (it == GESTURE_DEFINITIONS.end()) {
        std::cerr << "错误: 未知的手势类型" << std::endl;
        return false;
    }

    // 转换为关节命令并发送
    const auto& gesture_def = it->second;
    std::vector<JointCommand> joints = CreateJointsFromGesture(gesture_def, speed, torque);

    return hand.MoveJoints(joints);
}

// ========== 主程序 ==========

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  枭尧灵巧手 SDK - 预设手势演示        " << std::endl;
    std::cout << "========================================" << std::endl;

    DexHand hand;

    // 连接设备
    std::cout << "\n正在通过 EtherCAT 连接灵巧手..." << std::endl;
    bool success = hand.AutoConnect(CommType::ETHERCAT);

    if (!success) {
        std::cerr << "错误: 无法连接到灵巧手！" << std::endl;
        return 1;
    }

    std::cout << "✓ 成功连接到灵巧手！" << std::endl;

    // 设置控制模式为位置模式
    hand.SetControlMode(ControlMode::POSITION);

    // 定义要演示的手势列表
    const std::vector<GestureType> gesture_demo = {
        GestureType::OPEN_HAND,
        GestureType::FIST,
        GestureType::OK,
        GestureType::THUMBS_UP,
        GestureType::SIX_SIGN,
    };

    std::cout << "\n开始依次演示预设手势..." << std::endl;
    std::cout << "按 Ctrl+C 可随时停止演示\n" << std::endl;

    int cycle = 0;
    try {
        while (true) {
            cycle++;
            std::cout << "\n========== 第 " << cycle << " 轮演示 ==========" << std::endl;

            // 依次演示每个手势
            for (size_t i = 0; i < gesture_demo.size(); ++i) {
                GestureType gesture = gesture_demo[i];
                std::string gesture_name = GetGestureName(gesture);

                std::cout << "\n[" << (i + 1) << "/" << gesture_demo.size() << "] 执行手势: " << gesture_name << std::endl;

                // 执行手势
                if (!ExecuteGesture(hand, gesture, 100, 100)) {
                    std::cerr << "错误: 手势执行失败！" << std::endl;
                    hand.Disconnect();
                    return 1;
                }

                // 等待动作完成
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            }

            std::cout << "\n========== 第 " << cycle << " 轮演示完成 ==========" << std::endl;
            std::cout << "按 Ctrl+C 停止演示，或继续下一轮...\n" << std::endl;

            // 短暂延时后进入下一轮
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

    } catch (const std::exception& e) {
        std::cout << "\n程序被中断: " << e.what() << std::endl;
    }

    // 断开连接
    std::cout << "\n正在断开连接..." << std::endl;
    hand.Disconnect();
    std::cout << "✓ 已断开连接" << std::endl;

    std::cout << "\n演示结束，感谢使用！" << std::endl;
    return 0;
}
