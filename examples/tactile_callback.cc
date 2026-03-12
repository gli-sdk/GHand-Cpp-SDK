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
        for (int i = static_cast<int>(xiaoyao::FingerType::THUMB);
             i < static_cast<int>(xiaoyao::FingerType::NUM_FINGERS);
             i++) {
            xiaoyao::FingerType finger = static_cast<xiaoyao::FingerType>(i);
            xiaoyao::Force force = data.GetResultant(finger);
            std::cout << "  " << finger_names[i] << ": (x:" << force.x
                      << ", y:" << force.y << ", z:" << force.z << ") N" << std::endl;
        }
    });

    // 尝试通过ETHERCAT连接灵巧手
    std::cout << "Connecting to dexterous hand via EtherCAT..." << std::endl;
    bool success = hand.AutoConnect(xiaoyao::CommType::ETHERCAT);

    if (success) {
        std::cout << "Successfully connected to the dexterous hand!" << std::endl;

        // 打开触觉
        hand.OpenTactile();

        // 数据自动推送，无需轮询
        std::this_thread::sleep_for(std::chrono::seconds(30));

        hand.CloseTactile();
        hand.Disconnect();
        std::cout << "Connection closed." << std::endl;
    } else {
        std::cout << "Failed to connect to the dexterous hand!" << std::endl;
    }

    return 0;
}