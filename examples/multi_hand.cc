#include "xiaoyao/xiaoyao.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <sstream>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace xiaoyao;

// ========== 多灵巧手控制器 ==========

/**
 * @brief 单个灵巧手实例信息
 */
struct HandInstance {
    std::unique_ptr<DexHand> hand;        // 灵巧手实例
    std::string name;                     // 手的标识符（如 "hand_0"）
    DeviceInfo info;                      // 设备信息
    std::vector<Joint> joints_cache;      // 关节状态缓存（由回调更新）
    bool connected;                       // 连接状态

    HandInstance() : connected(false) {}
};

/**
 * @brief 多灵巧手控制器
 *
 * 自动连接多台灵巧手设备，并提供统一的控制接口
 * 使用回调机制缓存关节数据，避免需要同步的 GetJoints() 接口
 */
class MultiDexHandController {
public:
    MultiDexHandController() : initialized_(false) {}

    ~MultiDexHandController() {
        CloseAll();
    }

    /**
     * @brief 初始化控制器，自动搜索并连接所有可用设备
     * @return 至少成功连接一个设备返回true
     */
    bool Initialize() {
        std::cout << "\n=== 初始化多灵巧手控制器 ===" << std::endl;

        // 创建临时实例用于搜索适配器
        DexHand temp_hand;
        std::map<std::string, std::string> adapters = temp_hand.SearchAdapters();

        if (adapters.empty()) {
            std::cerr << "未找到可用的网络适配器" << std::endl;
            return false;
        }

        std::cout << "\n找到 " << adapters.size() << " 个可用适配器:" << std::endl;
        for (std::map<std::string, std::string>::const_iterator it = adapters.begin(); it != adapters.end(); ++it) {
            const std::string& name = it->first;
            const std::string& desc = it->second;
            std::cout << "  - " << name << ": " << desc << std::endl;
        }

        // 尝试连接每个适配器
        std::cout << "\n正在尝试连接设备..." << std::endl;
        int connected_count = 0;

        for (std::map<std::string, std::string>::const_iterator it = adapters.begin(); it != adapters.end(); ++it) {
            const std::string& adapter_name = it->first;
            const std::string& adapter_desc = it->second;
            std::cout << "  尝试连接: " << adapter_name << std::endl;

            auto instance = std::make_unique<HandInstance>();
            instance->hand = std::make_unique<DexHand>();

            // 尝试连接
            if (instance->hand->Connect(CommType::ETHERCAT, adapter_name)) {
                instance->name = "hand_" + std::to_string(connected_count);
                instance->connected = true;

                // 注册回调以缓存关节数据
                std::string name = instance->name;
                size_t index = hands_.size();
                instance->hand->SetJointsCallback([this, index](const std::vector<Joint>& joints) {
                    if (index < hands_.size() && hands_[index]) {
                        hands_[index]->joints_cache = joints;
                    }
                });

                // 读取设备信息
                instance->info = instance->hand->GetDeviceInfo();

                hands_.push_back(std::move(instance));
                connected_count++;

                std::cout << "    ✓ 连接成功 -> " << hands_.back()->name << std::endl;
            } else {
                std::cout << "    ✗ 连接失败" << std::endl;
            }
        }

        if (connected_count == 0) {
            std::cerr << "\n未能成功连接任何设备" << std::endl;
            return false;
        }

        std::cout << "\n✓ 成功连接 " << connected_count << " 台设备" << std::endl;

        // 显示所有设备信息
        std::cout << "\n设备信息:" << std::endl;
        for (const auto& instance : hands_) {
            std::cout << "  " << instance->name << ":" << std::endl;
            std::cout << "    设备名: " << instance->info.device_name << std::endl;
            std::cout << "    硬件版本: " << instance->info.hardware_version << std::endl;
            std::cout << "    软件版本: " << instance->info.software_version << std::endl;
            std::cout << "    序列号: " << instance->info.serial_number << std::endl;
        }

        initialized_ = true;
        return true;
    }

    /**
     * @brief 获取已连接手的数量
     */
    size_t GetHandCount() const {
        return hands_.size();
    }

    /**
     * @brief 获取指定手的名称
     */
    std::string GetHandName(size_t index) const {
        if (index >= hands_.size()) return "";
        return hands_[index]->name;
    }

    /**
     * @brief 控制指定的手
     * @param index 手的索引
     * @param joints 关节命令列表
     * @return 成功返回true
     */
    bool MoveHand(size_t index, const std::vector<JointCommand>& joints) {
        if (!initialized_) {
            std::cerr << "控制器未初始化" << std::endl;
            return false;
        }

        if (index >= hands_.size()) {
            std::cerr << "无效的手索引: " << index << std::endl;
            return false;
        }

        return hands_[index]->hand->MoveJoints(joints);
    }

    /**
     * @brief 同时控制所有手
     * @param joints_list 关节命令列表的列表，每个元素对应一只手
     * @return 全部成功返回true
     */
    bool MoveAllHands(const std::vector<std::vector<JointCommand>>& joints_list) {
        if (!initialized_) {
            std::cerr << "控制器未初始化" << std::endl;
            return false;
        }

        if (joints_list.size() != hands_.size()) {
            std::cerr << "关节数量不匹配: 有 " << hands_.size() << " 只手, "
                      << "但提供了 " << joints_list.size() << " 组关节命令" << std::endl;
            return false;
        }

        bool all_success = true;
        for (size_t i = 0; i < hands_.size(); ++i) {
            if (!MoveHand(i, joints_list[i])) {
                all_success = false;
            }
        }
        return all_success;
    }

    /**
     * @brief 停止所有手
     */
    void StopAll() {
        if (!initialized_) return;

        for (auto& instance : hands_) {
            instance->hand->Stop();
        }
    }

    /**
     * @brief 获取指定手的缓存关节数据
     * @param index 手的索引
     * @return 关节状态列表（const引用）
     */
    const std::vector<Joint>& GetJoints(size_t index) const {
        static const std::vector<Joint> empty_cache;
        if (index >= hands_.size()) return empty_cache;
        return hands_[index]->joints_cache;
    }

    /**
     * @brief 获取指定手的设备信息
     */
    DeviceInfo GetHandInfo(size_t index) const {
        if (index >= hands_.size()) {
            return DeviceInfo{};
        }
        return hands_[index]->info;
    }

    /**
     * @brief 关闭所有连接
     */
    void CloseAll() {
        if (!initialized_) return;

        std::cout << "\n正在关闭所有连接..." << std::endl;
        for (auto& instance : hands_) {
            if (instance->connected) {
                instance->hand->Disconnect();
                instance->connected = false;
                std::cout << "  ✓ 已关闭 " << instance->name << std::endl;
            }
        }
        hands_.clear();
        initialized_ = false;
        std::cout << "✓ 所有连接已关闭" << std::endl;
    }

private:
    std::vector<std::unique_ptr<HandInstance>> hands_;
    bool initialized_;
};

// ========== 辅助函数 ==========

/**
 * @brief 创建测试关节命令（弯曲食指）
 */
std::vector<JointCommand> CreateTestJoints() {
    return {
        {JointId::FF_PIP, 45.0f * M_PI / 180.0f, 100, 100},
        {JointId::FF_MCP, 30.0f * M_PI / 180.0f, 100, 100},
    };
}

/**
 * @brief 创建复位关节命令（所有关节归零）
 */
std::vector<JointCommand> CreateResetJoints() {
    return {
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
}

// ========== 主程序 ==========

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  枭尧灵巧手 SDK - 多手控制演示        " << std::endl;
    std::cout << "========================================" << std::endl;

    MultiDexHandController controller;

    // 初始化控制器（自动搜索并连接所有设备）
    if (!controller.Initialize()) {
        std::cerr << "\n初始化失败！" << std::endl;
        return 1;
    }

    std::cout << "\n✓ 控制器初始化成功" << std::endl;
    std::cout << "\n共连接 " << controller.GetHandCount() << " 台灵巧手" << std::endl;

    // 演示 1: 分别控制每只手
    std::cout << "\n========== 演示 1: 分别控制 ==========" << std::endl;

    std::vector<JointCommand> test_joints = CreateTestJoints();

    for (size_t i = 0; i < controller.GetHandCount(); ++i) {
        std::string hand_name = controller.GetHandName(i);
        std::cout << "\n控制 " << hand_name << "..." << std::endl;

        if (controller.MoveHand(i, test_joints)) {
            std::cout << "  ✓ 指令发送成功" << std::endl;

            // 等待一段时间让设备响应
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // 读取缓存的关节数据
            const std::vector<Joint>& joints = controller.GetJoints(i);
            if (!joints.empty()) {
                std::cout << "  ✓ 当前关节数: " << joints.size() << std::endl;
            } else {
                std::cout << "  ⚠ 关节数据缓存尚未更新" << std::endl;
            }
        } else {
            std::cout << "  ✗ 指令发送失败" << std::endl;
        }
    }

    // 保持姿势一段时间
    std::cout << "\n保持姿势 2 秒..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 演示 2: 同时控制所有手
    std::cout << "\n========== 演示 2: 同时控制 ==========" << std::endl;

    std::vector<JointCommand> reset_joints = CreateResetJoints();

    // 为每只手准备相同的复位命令
    std::vector<std::vector<JointCommand>> all_joints(controller.GetHandCount(), reset_joints);

    std::cout << "\n同时复位所有手..." << std::endl;
    if (controller.MoveAllHands(all_joints)) {
        std::cout << "  ✓ 所有手指令发送成功" << std::endl;
    } else {
        std::cout << "  ⚠ 部分手指令发送失败" << std::endl;
    }

    // 等待动作完成
    std::cout << "\n等待动作完成..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 显示最终的关节状态
    std::cout << "\n========== 最终关节状态 ==========" << std::endl;
    for (size_t i = 0; i < controller.GetHandCount(); ++i) {
        std::string hand_name = controller.GetHandName(i);
        const std::vector<Joint>& joints = controller.GetJoints(i);

        std::cout << hand_name << ": ";
        if (!joints.empty()) {
            std::cout << joints.size() << " 个关节" << std::endl;
        } else {
            std::cout << "数据缓存尚未更新" << std::endl;
        }
    }

    // 关闭所有连接
    std::cout << "\n========== 清理 ==========" << std::endl;
    controller.CloseAll();

    std::cout << "\n演示结束，感谢使用！" << std::endl;
    return 0;
}
