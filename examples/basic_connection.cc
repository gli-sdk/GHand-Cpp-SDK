#include <iostream>

#include "xiaoyao/dexhand.h"

int main() {
    DexHand hand;

    // 尝试通过ETHERCAT连接灵巧手
    std::cout << "Connecting to dexterous hand via EtherCAT..." << std::endl;
    int result = hand.Open(COMM_ETHERCAT, "auto");

    if (result == 0) {
        std::cout << "Successfully connected to the dexterous hand!" << std::endl;

        // 获取手部类型
        hand.GetHandType(1);
        HandType type = hand.hand_type_;
        if (type == HandType::LEFT) {
            std::cout << "Hand type: Left hand" << std::endl;
        } else if (type == HandType::RIGHT) {
            std::cout << "Hand type: Right hand" << std::endl;
        } else {
            std::cout << "Hand type: Unknown" << std::endl;
        }

        // 获取固件版本
        hand.GetDeviceInfo(1);
        std::string version = hand.device_info_.software_version;
        if (!version.empty()) {
            std::cout << "Firmware version: " << version << std::endl;
        }

        // 关闭连接
        hand.Close();
        std::cout << "Connection closed." << std::endl;
    } else {
        std::cout << "Failed to connect to the dexterous hand!" << std::endl;
    }

    return 0;
}