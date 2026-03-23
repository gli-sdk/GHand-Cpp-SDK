#include "xiaoyao/xiaoyao.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <string>
#include <sstream>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace xiaoyao;

/**
 * @brief 关节参数结构
 */
struct JointParams {
    float angle;   // 角度（度）
    int speed;     // 速度（0-100）
    int torque;    // 力矩（0-100）

    JointParams() : angle(0), speed(0), torque(0) {}
};

/**
 * @brief 显示关节ID列表
 */
void DisplayJointIdList() {
    std::cout << "\n关节ID列表:" << std::endl;
    std::cout << "  1: THUMB_PIP,      2: THUMB_MCP,      3: THUMB_SWING,     4: THUMB_ROTATION" << std::endl;
    std::cout << "  6: FF_PIP,         7: FF_MCP,         8: FF_SWING" << std::endl;
    std::cout << " 10: MF_PIP,        11: MF_MCP" << std::endl;
    std::cout << " 13: RF_PIP,        14: RF_MCP" << std::endl;
    std::cout << " 16: LF_PIP,        17: LF_MCP" << std::endl;
}

/**
 * @brief 显示已设置的关节参数
 */
void DisplaySetJoints(const std::unordered_map<int, JointParams>& joint_params) {
    if (joint_params.empty()) {
        return;
    }

    std::cout << "\n已设置的关节:" << std::endl;
    std::cout << std::left << std::setw(20) << "关节名称"
              << std::setw(10) << "角度(°)"
              << std::setw(10) << "速度(%)"
              << std::setw(10) << "力矩(%)" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    for (const auto& pair : joint_params) {
        int joint_id = pair.first;
        const JointParams& params = pair.second;

        // 获取关节名称
        std::string joint_name = ToString(static_cast<JointId>(joint_id));

        std::cout << std::left << std::setw(20) << joint_name
                  << std::fixed << std::setprecision(1) << std::setw(10) << params.angle
                  << std::setw(10) << params.speed
                  << std::setw(10) << params.torque << std::endl;
    }
}

/**
 * @brief 从用户读取数值，支持默认值
 * @param prompt 提示信息
 * @param default_value 默认值
 * @return 用户输入的值，如果直接回车则返回默认值
 */
float ReadFloatWithDefault(const std::string& prompt, float default_value) {
    std::cout << prompt << " [" << default_value << "]: ";
    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) {
        return default_value;
    }

    try {
        return std::stof(input);
    } catch (const std::exception&) {
        std::cout << "  输入无效，使用默认值: " << default_value << std::endl;
        return default_value;
    }
}

/**
 * @brief 从用户读取整数，支持默认值
 */
int ReadIntWithDefault(const std::string& prompt, int default_value) {
    std::cout << prompt << " [" << default_value << "]: ";
    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) {
        return default_value;
    }

    try {
        return std::stoi(input);
    } catch (const std::exception&) {
        std::cout << "  输入无效，使用默认值: " << default_value << std::endl;
        return default_value;
    }
}

/**
 * @brief 将关节ID映射到 JointId 枚举
 */
bool IsValidJointId(int id) {
    // 有效的关节ID列表
    static const int valid_ids[] = {
        1, 2, 3, 4,     // THUMB
        6, 7, 8,        // FF (Forefinger)
        10, 11,         // MF (Middle finger)
        13, 14,         // RF (Ring finger)
        16, 17          // LF (Little finger)
    };

    for (int valid_id : valid_ids) {
        if (id == valid_id) {
            return true;
        }
    }
    return false;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  枭尧灵巧手 SDK - 交互式关节控制    " << std::endl;
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

    // 显示设备信息
    DeviceInfo info = hand.GetDeviceInfo();
    HandType hand_type = hand.GetHandType();
    std::cout << "\n设备信息:" << std::endl;
    std::cout << "  设备名: " << info.device_name << std::endl;
    std::cout << "  硬件版本: " << info.hardware_version << std::endl;
    std::cout << "  软件版本: " << info.software_version << std::endl;
    std::cout << "  序列号: " << info.serial_number << std::endl;
    std::cout << "  手部类型: " << ToString(hand_type) << std::endl;

    // 设置控制模式为位置模式
    hand.SetControlMode(ControlMode::POSITION);

    // 注册关节数据回调（用于读取反馈）
    std::vector<Joint> last_joints;
    bool joints_received = false;
    hand.SetJointsCallback([&](const std::vector<Joint>& joints) {
        last_joints = joints;
        joints_received = true;
    });

    std::cout << "\n交互式控制模式已启动" << std::endl;
    std::cout << "按 Ctrl+C 可随时退出程序\n" << std::endl;

    try {
        while (true) {
            // 为每个控制循环重置关节参数
            std::unordered_map<int, JointParams> joint_params;

            DisplayJointIdList();
            std::cout << "\n请为关节设置参数 (输入空行结束输入):\n" << std::endl;

            // 交互式输入关节参数
            while (true) {
                // 显示已设置的关节
                DisplaySetJoints(joint_params);

                std::cout << "\n请输入关节ID (或直接按回车结束输入): ";
                std::string joint_input;
                std::getline(std::cin, joint_input);

                // 用户直接回车，结束输入
                if (joint_input.empty()) {
                    break;
                }

                // 解析关节ID
                try {
                    int joint_id = std::stoi(joint_input);

                    if (!IsValidJointId(joint_id)) {
                        std::cout << "  无效的关节ID，请重新输入" << std::endl;
                        continue;
                    }

                    // 获取当前参数（如果有）
                    JointParams& params = joint_params[joint_id];
                    std::string joint_name = ToString(static_cast<JointId>(joint_id));

                    std::cout << "\n为关节 " << joint_name << " 设置参数:" << std::endl;

                    // 读取角度、速度、力矩
                    params.angle = ReadFloatWithDefault("角度值(度)", params.angle);
                    params.speed = ReadIntWithDefault("速度值(0-100)", params.speed);
                    params.torque = ReadIntWithDefault("力矩值(0-100)", params.torque);

                    std::cout << "  ✓ 已设置" << std::endl;

                } catch (const std::exception& e) {
                    std::cout << "  输入格式错误: " << e.what() << std::endl;
                }
            }

            // 构建关节命令列表
            std::vector<JointCommand> joints;

            if (joint_params.empty()) {
                std::cout << "\n未设置任何关节，所有关节将保持当前位置" << std::endl;
            } else {
                for (const auto& pair : joint_params) {
                    int joint_id = pair.first;
                    const JointParams& params = pair.second;

                    // 转换角度从度到弧度
                    float angle_rad = params.angle * M_PI / 180.0f;

                    joints.push_back({
                        static_cast<JointId>(joint_id),
                        angle_rad,
                        static_cast<uint8_t>(params.speed),
                        static_cast<uint8_t>(params.torque)
                    });
                }

                std::cout << "\n发送关节命令..." << std::endl;
            }

            // 发送关节命令
            bool move_success = hand.MoveJoints(joints);

            if (move_success) {
                std::cout << "✓ 命令发送成功" << std::endl;

                // 等待一段时间让设备响应并获取关节数据
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                // 显示当前关节状态（如果有回调数据）
                if (joints_received && !last_joints.empty()) {
                    std::cout << "\n当前关节状态 (部分显示):" << std::endl;
                    std::cout << std::left << std::setw(20) << "关节"
                              << std::setw(12) << "角度(度)"
                              << "状态" << std::endl;
                    std::cout << std::string(40, '-') << std::endl;

                    // 只显示前5个关节作为示例
                    for (size_t i = 0; i < std::min(size_t(5), last_joints.size()); ++i) {
                        const Joint& joint = last_joints[i];
                        float angle_deg = joint.angle * 180.0f / M_PI;
                        std::cout << std::left << std::setw(20) << ToString(joint.id)
                                  << std::fixed << std::setprecision(1) << std::setw(12) << angle_deg
                                  << ToString(joint.state) << std::endl;
                    }
                    if (last_joints.size() > 5) {
                        std::cout << "... (还有 " << (last_joints.size() - 5) << " 个关节)" << std::endl;
                    }
                }
            } else {
                std::cerr << "✗ 命令发送失败" << std::endl;
            }

            std::cout << "\n" << std::string(50, '=') << std::endl;
            std::cout << "按回车键开始下一轮控制..." << std::endl;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }

    } catch (const std::exception& e) {
        std::cout << "\n程序异常: " << e.what() << std::endl;
    }

    // 断开连接
    std::cout << "\n正在断开连接..." << std::endl;
    hand.Disconnect();
    std::cout << "✓ 已断开连接" << std::endl;

    std::cout << "\n程序结束，感谢使用！" << std::endl;
    return 0;
}
