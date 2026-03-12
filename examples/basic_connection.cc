#include <iostream>

#include "xiaoyao/dexhand.h"
using namespace xiaoyao;

int main() {
    DexHand hand;

    // 尝试通过ETHERCAT连接灵巧手
    std::cout << "Connecting to dexterous hand via EtherCAT..." << std::endl;
    int result = hand.Open(COMM_ETHERCAT, "auto");

    if (result >= 0) {
        std::cout << "Successfully connected to the dexterous hand!" << std::endl;

        // 获取手部类型
        HandType type = hand.GetHandType();
        std::cout << "Hand type: " <<ToString(type) << std::endl;

        // 获取固件版本
        DeviceInfo device_info = hand.GetDeviceInfo();
        std::string version = device_info.software_version;
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