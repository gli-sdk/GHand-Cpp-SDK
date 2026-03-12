#include "xiaoyao/dexhand.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    xiaoyao::DexHand hand;

    // 注册触觉数据回调（一次性）
    hand.SetTactileDataCallback([](const xiaoyao::TactileData& data) {
        if (data.type == xiaoyao::ForceType::RESULTANT &&
            data.finger == xiaoyao::THUMB) {

            if (!data.forces.empty()) {
                const auto& f = data.forces[0];
                float mag = std::sqrt(f.x*f.x + f.y*f.y + f.z*f.z);
                std::cout << "Thumb force: " << mag << " N" << std::endl;
            }

        } else if (data.type == xiaoyao::ForceType::SAMPLE &&
                   data.finger == xiaoyao::FF) {

            std::cout << "Index sensors: " << data.SensorCount() << " sensors" << std::endl;
        }
    });

    // 打开连接
    int result = hand.Open(xiaoyao::COMM_ETHERCAT, "auto");

    if (result >= 0) {
        std::cout << "Successfully connected to the dexterous hand!" << std::endl;

        // 获取手部类型
        xiaoyao::HandType type = hand.GetHandType();
        if (type == xiaoyao::LEFT) {
            std::cout << "Hand type: Left hand" << std::endl;
        } else if (type == xiaoyao::RIGHT) {
            std::cout << "Hand type: Right hand" << std::endl;
        } else {
            std::cout << "Hand type: Unknown" << std::endl;
        }

        // 获取固件版本
        xiaoyao::DeviceInfo device_info = hand.GetDeviceInfo();
        std::string version = device_info.software_version;
        if (!version.empty()) {
            std::cout << "Firmware version: " << version << std::endl;
        }

        // 数据自动推送，无需轮询
        std::this_thread::sleep_for(std::chrono::seconds(30));

        hand.Close();
        std::cout << "Connection closed." << std::endl;
    } else {
        std::cout << "Failed to connect to the dexterous hand!" << std::endl;
    }

    return 0;
}