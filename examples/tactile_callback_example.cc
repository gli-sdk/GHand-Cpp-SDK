#include "xiaoyao/dexhand.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    xiaoyao::DexHand hand;

    // 注册触觉数据回调（一次性）
    hand.SetTactileDataCallback([](const xiaoyao::TactileData& data) {
        // 打印所有手指的合力
        std::cout << "All fingers resultant forces:" << std::endl;
        const char* finger_names[] = {"Thumb", "Index", "Middle", "Ring", "Little"};
        for (int i = xiaoyao::THUMB; i < xiaoyao::NUM_FINGERS; i++) {
            xiaoyao::FingerType finger = static_cast<xiaoyao::FingerType>(i);
            xiaoyao::Force force = data.GetResultant(finger);
            std::cout << "  " << finger_names[i] << ": (x:" << force.x
                      << ", y:" << force.y << ", z:" << force.z << ") N" << std::endl;
        }
    });

    // 尝试通过ETHERCAT连接灵巧手
    std::cout << "Connecting to dexterous hand via EtherCAT..." << std::endl;
    bool success = hand.Connect(xiaoyao::COMM_ETHERCAT, "auto");

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

        // 数据自动推送，无需轮询
        std::this_thread::sleep_for(std::chrono::seconds(30));

        hand.Disconnect();
        std::cout << "Connection closed." << std::endl;
    } else {
        std::cout << "Failed to connect to the dexterous hand!" << std::endl;
    }

    return 0;
}