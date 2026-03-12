#include "xiaoyao/dexhand.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

// 启用 ANSI 转义序列支持（Windows）
void EnableAnsiColors() {
#ifdef _WIN32
    // 设置控制台代码页为 UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}

int main() {
    // 启用 ANSI 转义序列
    EnableAnsiColors();

    xiaoyao::DexHand hand;

    // ANSI 转义序列用于固定位置显示
    // \033[H: 移动到屏幕左上角
    // \033[J: 清除从光标到屏幕末尾
    // \033[row;colH: 移动到指定行列
    const char* CLEAR_SCREEN = "\033[H\033[J";
    const char* MOVE_CURSOR = "\033[H";  // 移动到左上角

    bool first_print = true;

    // 注册触觉数据回调（一次性）
    hand.SetTactileDataCallback([&first_print, CLEAR_SCREEN, MOVE_CURSOR](const xiaoyao::TactileData& data) {
        const char* finger_names[] = {"Thumb", "Index", "Middle", "Ring", "Little"};

        if (first_print) {
            // 第一次打印，显示标题
            std::cout << CLEAR_SCREEN;
            std::cout << "+==================================================+" << std::endl;
            std::cout << "|       Tactile Data - Real-time Display          |" << std::endl;
            std::cout << "+--------------------------------------------------+" << std::endl;
            first_print = false;
        } else {
            // 后续更新，移动光标到数据区域
            std::cout << MOVE_CURSOR;
            std::cout << "\033[4H";  // 移动到第4行开始
        }

        // 打印所有手指的合力
        for (int i = static_cast<int>(xiaoyao::FingerType::THUMB);
             i < static_cast<int>(xiaoyao::FingerType::NUM_FINGERS);
             i++) {
            xiaoyao::FingerType finger = static_cast<xiaoyao::FingerType>(i);
            xiaoyao::Force force = data.GetResultant(finger);

            std::cout << "| " << std::setw(6) << finger_names[i] << ": "
                      << "x=" << std::setw(6) << std::fixed << std::setprecision(2) << force.x
                      << ", y=" << std::setw(6) << std::fixed << std::setprecision(2) << force.y
                      << ", z=" << std::setw(6) << std::fixed << std::setprecision(2) << force.z
                      << " N |" << std::endl;
        }

        std::cout << "+==================================================+" << std::endl;
        std::cout << std::flush;  // 确保立即输出
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