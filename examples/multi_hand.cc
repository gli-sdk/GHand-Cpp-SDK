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

// ========== Multi-Dexterous Hand Controller ==========

/**
 * @brief Single dexterous hand instance information
 */
struct HandInstance {
    std::unique_ptr<DexHand> hand;        // Dexterous hand instance
    std::string name;                     // Hand identifier (e.g., "hand_0")
    DeviceInfo info;                      // Device information
    std::vector<Joint> joints_cache;      // Joint status cache (updated by callback)
    bool connected;                       // Connection status

    HandInstance() : connected(false) {}
};

/**
 * @brief Multi-dexterous hand controller
 *
 * Automatically connects multiple dexterous hand devices and provides unified control interface
 * Uses callback mechanism to cache joint data, avoiding synchronous GetJoints() interface
 */
class MultiDexHandController {
public:
    MultiDexHandController() : initialized_(false) {}

    ~MultiDexHandController() {
        CloseAll();
    }

    /**
     * @brief Initialize controller, automatically search and connect all available devices
     * @return Returns true if at least one device is successfully connected
     */
    bool Initialize() {
        std::cout << "\n=== Initializing Multi-Dexterous Hand Controller ===" << std::endl;

        // Create temporary instance for searching adapters
        DexHand temp_hand;
        std::map<std::string, std::string> adapters = temp_hand.SearchAdapters();

        if (adapters.empty()) {
            std::cerr << "No available network adapters found" << std::endl;
            return false;
        }

        std::cout << "\nFound " << adapters.size() << " available adapters:" << std::endl;
        for (std::map<std::string, std::string>::const_iterator it = adapters.begin(); it != adapters.end(); ++it) {
            const std::string& name = it->first;
            const std::string& desc = it->second;
            std::cout << "  - " << name << ": " << desc << std::endl;
        }

        // Try to connect each adapter
        std::cout << "\nAttempting to connect devices..." << std::endl;
        int connected_count = 0;

        for (std::map<std::string, std::string>::const_iterator it = adapters.begin(); it != adapters.end(); ++it) {
            const std::string& adapter_name = it->first;
            const std::string& adapter_desc = it->second;
            std::cout << "  Attempting connection: " << adapter_name << std::endl;

            auto instance = std::make_unique<HandInstance>();
            instance->hand = std::make_unique<DexHand>();

            // Try to connect
            if (instance->hand->Connect(CommType::ETHERCAT, adapter_name)) {
                instance->name = "hand_" + std::to_string(connected_count);
                instance->connected = true;

                // Register callback to cache joint data
                std::string name = instance->name;
                size_t index = hands_.size();
                instance->hand->SetJointsCallback([this, index](const std::vector<Joint>& joints) {
                    if (index < hands_.size() && hands_[index]) {
                        hands_[index]->joints_cache = joints;
                    }
                });

                // Read device information
                instance->info = instance->hand->GetDeviceInfo();

                hands_.push_back(std::move(instance));
                connected_count++;

                std::cout << "    ✓ Connected successfully -> " << hands_.back()->name << std::endl;
            } else {
                std::cout << "    ✗ Connection failed" << std::endl;
            }
        }

        if (connected_count == 0) {
            std::cerr << "\nFailed to connect any device" << std::endl;
            return false;
        }

        std::cout << "\n✓ Successfully connected " << connected_count << " devices" << std::endl;

        // Display all device information
        std::cout << "\nDevice Information:" << std::endl;
        for (const auto& instance : hands_) {
            std::cout << "  " << instance->name << ":" << std::endl;
            std::cout << "    Device Name: " << instance->info.device_name << std::endl;
            std::cout << "    Hardware Version: " << instance->info.hardware_version << std::endl;
            std::cout << "    Software Version: " << instance->info.software_version << std::endl;
            std::cout << "    Serial Number: " << instance->info.serial_number << std::endl;
        }

        initialized_ = true;
        return true;
    }

    /**
     * @brief Get number of connected hands
     */
    size_t GetHandCount() const {
        return hands_.size();
    }

    /**
     * @brief Get name of specified hand
     */
    std::string GetHandName(size_t index) const {
        if (index >= hands_.size()) return "";
        return hands_[index]->name;
    }

    /**
     * @brief Control specified hand
     * @param index Hand index
     * @param joints Joint command list
     * @return Returns true on success
     */
    bool MoveHand(size_t index, const std::vector<JointCommand>& joints) {
        if (!initialized_) {
            std::cerr << "Controller not initialized" << std::endl;
            return false;
        }

        if (index >= hands_.size()) {
            std::cerr << "Invalid hand index: " << index << std::endl;
            return false;
        }

        return hands_[index]->hand->MoveJoints(joints);
    }

    /**
     * @brief Control all hands simultaneously
     * @param joints_list List of joint command lists, each element corresponds to one hand
     * @return Returns true if all succeed
     */
    bool MoveAllHands(const std::vector<std::vector<JointCommand>>& joints_list) {
        if (!initialized_) {
            std::cerr << "Controller not initialized" << std::endl;
            return false;
        }

        if (joints_list.size() != hands_.size()) {
            std::cerr << "Joint count mismatch: have " << hands_.size() << " hands, "
                      << "but provided " << joints_list.size() << " joint command groups" << std::endl;
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
     * @brief Stop all hands
     */
    void StopAll() {
        if (!initialized_) return;

        for (auto& instance : hands_) {
            instance->hand->Stop();
        }
    }

    /**
     * @brief Get cached joint data of specified hand
     * @param index Hand index
     * @return Joint status list (const reference)
     */
    const std::vector<Joint>& GetJoints(size_t index) const {
        static const std::vector<Joint> empty_cache;
        if (index >= hands_.size()) return empty_cache;
        return hands_[index]->joints_cache;
    }

    /**
     * @brief Get device information of specified hand
     */
    DeviceInfo GetHandInfo(size_t index) const {
        if (index >= hands_.size()) {
            return DeviceInfo{};
        }
        return hands_[index]->info;
    }

    /**
     * @brief Close all connections
     */
    void CloseAll() {
        if (!initialized_) return;

        std::cout << "\nClosing all connections..." << std::endl;
        for (auto& instance : hands_) {
            if (instance->connected) {
                instance->hand->Disconnect();
                instance->connected = false;
                std::cout << "  ✓ Closed " << instance->name << std::endl;
            }
        }
        hands_.clear();
        initialized_ = false;
        std::cout << "✓ All connections closed" << std::endl;
    }

private:
    std::vector<std::unique_ptr<HandInstance>> hands_;
    bool initialized_;
};

// ========== Helper Functions ==========

/**
 * @brief Create test joint commands (bend index finger)
 */
std::vector<JointCommand> CreateTestJoints() {
    return {
        {JointId::FF_PIP, 45.0f, 100, 100},
        {JointId::FF_MCP, 30.0f, 100, 100},
    };
}

/**
 * @brief Create reset joint commands (reset all joints to zero)
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

// ========== Main Program ==========

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Xiaoyao Dexterous Hand SDK - Multi-Hand Control Demo        " << std::endl;
    std::cout << "========================================" << std::endl;

    MultiDexHandController controller;

    // Initialize controller (automatically search and connect all devices)
    if (!controller.Initialize()) {
        std::cerr << "\nInitialization failed!" << std::endl;
        return 1;
    }

    std::cout << "\n✓ Controller initialized successfully" << std::endl;
    std::cout << "\nTotal connected dexterous hands: " << controller.GetHandCount() << std::endl;

    // Demo 1: Control each hand separately
    std::cout << "\n========== Demo 1: Separate Control ==========" << std::endl;

    std::vector<JointCommand> test_joints = CreateTestJoints();

    for (size_t i = 0; i < controller.GetHandCount(); ++i) {
        std::string hand_name = controller.GetHandName(i);
        std::cout << "\nControlling " << hand_name << "..." << std::endl;

        if (controller.MoveHand(i, test_joints)) {
            std::cout << "  ✓ Command sent successfully" << std::endl;

            // Wait for device to respond
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Read cached joint data
            const std::vector<Joint>& joints = controller.GetJoints(i);
            if (!joints.empty()) {
                std::cout << "  ✓ Current joint count: " << joints.size() << std::endl;
            } else {
                std::cout << "  ⚠ Joint data cache not yet updated" << std::endl;
            }
        } else {
            std::cout << "  ✗ Command send failed" << std::endl;
        }
    }

    // Hold position for a while
    std::cout << "\nHolding position for 2 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Demo 2: Control all hands simultaneously
    std::cout << "\n========== Demo 2: Simultaneous Control ==========" << std::endl;

    std::vector<JointCommand> reset_joints = CreateResetJoints();

    // Prepare same reset command for each hand
    std::vector<std::vector<JointCommand>> all_joints(controller.GetHandCount(), reset_joints);

    std::cout << "\nResetting all hands simultaneously..." << std::endl;
    if (controller.MoveAllHands(all_joints)) {
        std::cout << "  ✓ All hand commands sent successfully" << std::endl;
    } else {
        std::cout << "  ⚠ Some hand commands failed" << std::endl;
    }

    // Wait for movement completion
    std::cout << "\nWaiting for movement completion..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Display final joint status
    std::cout << "\n========== Final Joint Status ==========" << std::endl;
    for (size_t i = 0; i < controller.GetHandCount(); ++i) {
        std::string hand_name = controller.GetHandName(i);
        const std::vector<Joint>& joints = controller.GetJoints(i);

        std::cout << hand_name << ": ";
        if (!joints.empty()) {
            std::cout << joints.size() << " joints" << std::endl;
        } else {
            std::cout << "Data cache not yet updated" << std::endl;
        }
    }

    // Close all connections
    std::cout << "\n========== Cleanup ==========" << std::endl;
    controller.CloseAll();

    std::cout << "\nDemo completed. Thank you for using!" << std::endl;
    return 0;
}
