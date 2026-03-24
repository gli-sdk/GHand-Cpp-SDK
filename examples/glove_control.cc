// 在包含 Windows 头文件之前先定义这些宏，避免 winsock.h/winsock2.h 冲突
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define closesocket close
    typedef int SOCKET;
    const int INVALID_SOCKET = -1;
    const int SOCKET_ERROR = -1;
#endif

#include "xiaoyao/xiaoyao.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cstring>
#include <string>

using namespace xiaoyao;

// ========== 数据结构 ==========

/**
 * @brief 大拇指手指数据（9个传感器）
 */
struct ThumbFinger {
    float mcp_bend, mcp_sway, mcp_roll;
    float pip_bend, pip_sway, pip_roll;
    float dip_bend, dip_sway, dip_roll;

    ThumbFinger() : mcp_bend(0), mcp_sway(0), mcp_roll(0),
                    pip_bend(0), pip_sway(0), pip_roll(0),
                    dip_bend(0), dip_sway(0), dip_roll(0) {}
};

/**
 * @brief 其他手指数据（4个传感器）
 */
struct Finger {
    float mcp_bend, mcp_sway;
    float pip_bend, pip_sway;

    Finger() : mcp_bend(0), mcp_sway(0), pip_bend(0), pip_sway(0) {}
};

/**
 * @brief 手部数据
 */
struct HandData {
    ThumbFinger thumb;
    Finger index;
    Finger middle;
    Finger ring;
    Finger pinky;
};

// ========== 配置参数 ==========

const char* UDP_IP = "192.168.1.19";
const int UDP_PORT = 8080;
const double PROCESS_INTERVAL = 0.02;  // 20ms 处理间隔

// ========== 辅助函数 ==========

/**
 * @brief 角度限制函数（将角度限制在指定范围内）
 * @param value 输入角度值（度）
 * @param min_angle 最小角度值（度）
 * @param max_angle 最大角度值（度）
 * @return 被限制在范围内的角度值（度）
 */
float ClipAngle(float value, float min_angle, float max_angle) {
    float clamped = value;
    if (clamped < min_angle) clamped = min_angle;
    if (clamped > max_angle) clamped = max_angle;
    return clamped;
}

/**
 * @brief 处理手套数据，解析左右手的手指数据
 * @param data 原始数据字符串
 * @param left_hand 输出：左手数据
 * @param right_hand 输出：右手数据
 * @return 成功返回true
 */
bool ProcessGloveData(const char* data, HandData& left_hand, HandData& right_hand) {
    // 复制数据以避免修改原始字符串
    std::string data_str(data);
    std::vector<float> numeric_data;

    // 手动解析逗号分隔的数据
    size_t start = 0;
    size_t end = data_str.find(',');
    bool first_item = true;

    while (end != std::string::npos) {
        std::string item = data_str.substr(start, end - start);

        if (!first_item) {
            try {
                float value = std::stof(item);
                numeric_data.push_back(value);
            } catch (const std::exception&) {
                // 转换失败，跳过
            }
        } else {
            first_item = false;
        }

        start = end + 1;
        end = data_str.find(',', start);
    }

    // 处理最后一个项目
    if (start < data_str.length() && !first_item) {
        try {
            float value = std::stof(data_str.substr(start));
            numeric_data.push_back(value);
        } catch (const std::exception&) {
            // 转换失败，跳过
        }
    }

    // 需要至少 192 个数据点
    if (numeric_data.size() < 192) {
        return false;
    }

    // 提取右手数据
    std::vector<float> right_hand_data;
    // 右手拇指 (组 1, 2, 3)
    right_hand_data.push_back(numeric_data[10]); right_hand_data.push_back(numeric_data[9]);
    right_hand_data.push_back(numeric_data[11]);  // mcp bend, sway, roll
    right_hand_data.push_back(numeric_data[16]); right_hand_data.push_back(numeric_data[15]);
    right_hand_data.push_back(numeric_data[17]);  // pip bend, sway, roll
    right_hand_data.push_back(numeric_data[22]); right_hand_data.push_back(numeric_data[21]);
    right_hand_data.push_back(numeric_data[23]);  // dip bend, sway, roll

    // 右手食指 (组 4, 5)
    right_hand_data.push_back(numeric_data[28]); right_hand_data.push_back(numeric_data[27]);  // mcp
    right_hand_data.push_back(numeric_data[34]); right_hand_data.push_back(numeric_data[33]);  // pip

    // 右手中指 (组 7, 8)
    right_hand_data.push_back(numeric_data[46]); right_hand_data.push_back(numeric_data[45]);  // mcp
    right_hand_data.push_back(numeric_data[52]); right_hand_data.push_back(numeric_data[51]);  // pip

    // 右手无名指 (组 10, 11)
    right_hand_data.push_back(numeric_data[64]); right_hand_data.push_back(numeric_data[63]);  // mcp
    right_hand_data.push_back(numeric_data[70]); right_hand_data.push_back(numeric_data[69]);  // pip

    // 右手小指 (组 13, 14)
    right_hand_data.push_back(numeric_data[82]); right_hand_data.push_back(numeric_data[81]);  // mcp
    right_hand_data.push_back(numeric_data[88]); right_hand_data.push_back(numeric_data[87]);  // pip

    // 提取左手数据
    std::vector<float> left_hand_data;
    // 左手拇指 (组 17, 18, 19)
    left_hand_data.push_back(numeric_data[106]); left_hand_data.push_back(numeric_data[105]);
    left_hand_data.push_back(numeric_data[107]);  // mcp bend, sway, roll
    left_hand_data.push_back(numeric_data[112]); left_hand_data.push_back(numeric_data[111]);
    left_hand_data.push_back(numeric_data[113]);  // pip bend, sway, roll
    left_hand_data.push_back(numeric_data[118]); left_hand_data.push_back(numeric_data[117]);
    left_hand_data.push_back(numeric_data[119]);  // dip bend, sway, roll

    // 左手食指 (组 20, 21)
    left_hand_data.push_back(numeric_data[124]); left_hand_data.push_back(numeric_data[123]);  // mcp
    left_hand_data.push_back(numeric_data[130]); left_hand_data.push_back(numeric_data[129]);  // pip

    // 左手中指 (组 23, 24)
    left_hand_data.push_back(numeric_data[142]); left_hand_data.push_back(numeric_data[141]);  // mcp
    left_hand_data.push_back(numeric_data[148]); left_hand_data.push_back(numeric_data[147]);  // pip

    // 左手无名指 (组 26, 27)
    left_hand_data.push_back(numeric_data[160]); left_hand_data.push_back(numeric_data[159]);  // mcp
    left_hand_data.push_back(numeric_data[166]); left_hand_data.push_back(numeric_data[165]);  // pip

    // 左手小指 (组 29, 30)
    left_hand_data.push_back(numeric_data[178]); left_hand_data.push_back(numeric_data[177]);  // mcp
    left_hand_data.push_back(numeric_data[184]); left_hand_data.push_back(numeric_data[183]);  // pip

    // 构建右手数据对象
    if (right_hand_data.size() >= 25) {
        right_hand.thumb.mcp_bend = right_hand_data[0];
        right_hand.thumb.mcp_sway = right_hand_data[1];
        right_hand.thumb.mcp_roll = right_hand_data[2];
        right_hand.thumb.pip_bend = right_hand_data[3];
        right_hand.thumb.pip_sway = right_hand_data[4];
        right_hand.thumb.pip_roll = right_hand_data[5];
        right_hand.thumb.dip_bend = right_hand_data[6];
        right_hand.thumb.dip_sway = right_hand_data[7];
        right_hand.thumb.dip_roll = right_hand_data[8];

        right_hand.index.mcp_bend = right_hand_data[9];
        right_hand.index.mcp_sway = right_hand_data[10];
        right_hand.index.pip_bend = right_hand_data[11];
        right_hand.index.pip_sway = right_hand_data[12];

        right_hand.middle.mcp_bend = right_hand_data[13];
        right_hand.middle.mcp_sway = right_hand_data[14];
        right_hand.middle.pip_bend = right_hand_data[15];
        right_hand.middle.pip_sway = right_hand_data[16];

        right_hand.ring.mcp_bend = right_hand_data[17];
        right_hand.ring.mcp_sway = right_hand_data[18];
        right_hand.ring.pip_bend = right_hand_data[19];
        right_hand.ring.pip_sway = right_hand_data[20];

        right_hand.pinky.mcp_bend = right_hand_data[21];
        right_hand.pinky.mcp_sway = right_hand_data[22];
        right_hand.pinky.pip_bend = right_hand_data[23];
        right_hand.pinky.pip_sway = right_hand_data[24];
    }

    // 构建左手数据对象
    if (left_hand_data.size() >= 25) {
        left_hand.thumb.mcp_bend = left_hand_data[0];
        left_hand.thumb.mcp_sway = left_hand_data[1];
        left_hand.thumb.mcp_roll = left_hand_data[2];
        left_hand.thumb.pip_bend = left_hand_data[3];
        left_hand.thumb.pip_sway = left_hand_data[4];
        left_hand.thumb.pip_roll = left_hand_data[5];
        left_hand.thumb.dip_bend = left_hand_data[6];
        left_hand.thumb.dip_sway = left_hand_data[7];
        left_hand.thumb.dip_roll = left_hand_data[8];

        left_hand.index.mcp_bend = left_hand_data[9];
        left_hand.index.mcp_sway = left_hand_data[10];
        left_hand.index.pip_bend = left_hand_data[11];
        left_hand.index.pip_sway = left_hand_data[12];

        left_hand.middle.mcp_bend = left_hand_data[13];
        left_hand.middle.mcp_sway = left_hand_data[14];
        left_hand.middle.pip_bend = left_hand_data[15];
        left_hand.middle.pip_sway = left_hand_data[16];

        left_hand.ring.mcp_bend = left_hand_data[17];
        left_hand.ring.mcp_sway = left_hand_data[18];
        left_hand.ring.pip_bend = left_hand_data[19];
        left_hand.ring.pip_sway = left_hand_data[20];

        left_hand.pinky.mcp_bend = left_hand_data[21];
        left_hand.pinky.mcp_sway = left_hand_data[22];
        left_hand.pinky.pip_bend = left_hand_data[23];
        left_hand.pinky.pip_sway = left_hand_data[24];
    }

    return true;
}

// ========== 主程序 ==========

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  枭尧灵巧手 SDK - 数据手套控制        " << std::endl;
    std::cout << "========================================" << std::endl;

#ifdef _WIN32
    // 初始化 Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "错误: WSAStartup 失败" << std::endl;
        return 1;
    }
#endif

    // 创建 UDP socket
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "错误: 无法创建 socket" << std::endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    // 绑定 socket
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(UDP_PORT);

    if (bind(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "错误: 无法绑定到 " << UDP_IP << ":" << UDP_PORT << std::endl;
        closesocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    std::cout << "\n✓ 正在监听 " << UDP_IP << ":" << UDP_PORT << " 上的数据..." << std::endl;

    // 连接灵巧手
    DexHand hand;
    std::cout << "\n正在通过 EtherCAT 连接灵巧手..." << std::endl;
    bool success = hand.AutoConnect(CommType::ETHERCAT);

    if (!success) {
        std::cerr << "错误: 无法连接到灵巧手！" << std::endl;
        closesocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    std::cout << "✓ 成功连接到灵巧手！" << std::endl;
    std::cout << "\n开始接收手套数据并控制灵巧手..." << std::endl;
    std::cout << "按 Ctrl+C 退出程序\n" << std::endl;

    // 设置接收超时，避免永久阻塞
#ifdef _WIN32
    DWORD timeout = 1000;  // 1秒超时
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    // 注册关节数据回调（用于显示反馈）
    std::vector<Joint> last_joints;
    bool joints_received = false;
    hand.SetJointsCallback([&](const std::vector<Joint>& joints) {
        last_joints = joints;
        joints_received = true;
    });

    auto last_process_time = std::chrono::steady_clock::now();
    int data_count = 0;

    try {
        while (true) {
            // 接收 UDP 数据
            char buffer[32 * 1024];
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            int recv_len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                                    (sockaddr*)&client_addr, &client_len);

            if (recv_len == SOCKET_ERROR) {
#ifdef _WIN32
                int error = WSAGetLastError();
                if (error == WSAETIMEDOUT) {
                    // 超时，继续循环
                    continue;
                }
#endif
                std::cerr << "错误: 接收数据失败" << std::endl;
                break;
            }

            buffer[recv_len] = '\0';

            // 检查是否需要处理数据
            auto current_time = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(current_time - last_process_time).count();

            if (elapsed >= PROCESS_INTERVAL) {
                HandData left_hand, right_hand;

                if (ProcessGloveData(buffer, left_hand, right_hand)) {
                    data_count++;

                    // 每 50 帧显示一次手套数据
                    if (data_count % 50 == 0) {
                        std::cout << "【手套数据】左手拇指 MCP: bend=" << left_hand.thumb.mcp_bend
                                  << ", sway=" << left_hand.thumb.mcp_sway
                                  << ", roll=" << left_hand.thumb.mcp_roll << std::endl;
                    }

                    // 使用左手数据控制灵巧手
                    std::vector<JointCommand> joints;
                    const int speed = 100;
                    const int torque = 100;

                    // 拇指关节
                    joints.push_back({JointId::THUMB_PIP,
                        ClipAngle(left_hand.thumb.pip_bend, 0, 75), speed, torque});
                    joints.push_back({JointId::THUMB_MCP,
                        ClipAngle(left_hand.thumb.mcp_bend - 40, 0, 55), speed, torque});
                    joints.push_back({JointId::THUMB_SWING,
                        ClipAngle(-(left_hand.thumb.mcp_roll + left_hand.thumb.pip_roll +
                                          left_hand.thumb.dip_roll) - 85, 0, 90), speed, torque});
                    joints.push_back({JointId::THUMB_ROTATION,
                        ClipAngle(-left_hand.thumb.dip_sway, -30, 60), speed, torque});

                    // 食指关节
                    joints.push_back({JointId::FF_PIP,
                        ClipAngle(left_hand.index.pip_bend, 0, 75), speed, torque});
                    joints.push_back({JointId::FF_MCP,
                        ClipAngle(left_hand.index.mcp_bend, 0, 70), speed, torque});
                    joints.push_back({JointId::FF_SWING,
                        ClipAngle(left_hand.index.mcp_sway + left_hand.index.pip_sway, -15, 15), speed, torque});

                    // 中指关节
                    joints.push_back({JointId::MF_PIP,
                        ClipAngle(left_hand.middle.pip_bend, 0, 75), speed, torque});
                    joints.push_back({JointId::MF_MCP,
                        ClipAngle(left_hand.middle.mcp_bend, 0, 70), speed, torque});

                    // 无名指关节
                    joints.push_back({JointId::RF_PIP,
                        ClipAngle(left_hand.ring.pip_bend, 0, 75), speed, torque});
                    joints.push_back({JointId::RF_MCP,
                        ClipAngle(left_hand.ring.mcp_bend, 0, 70), speed, torque});

                    // 小指关节
                    joints.push_back({JointId::LF_PIP,
                        ClipAngle(left_hand.pinky.pip_bend, 0, 75), speed, torque});
                    joints.push_back({JointId::LF_MCP,
                        ClipAngle(left_hand.pinky.mcp_bend, 0, 70), speed, torque});

                    // 发送关节命令
                    hand.MoveJoints(joints);

                    // 每 100 帧显示一次关节状态
                    if (data_count % 100 == 0 && joints_received && !last_joints.empty()) {
                        std::cout << "【灵巧手状态】已处理 " << data_count << " 帧数据" << std::endl;
                    }
                }

                last_process_time = current_time;
            }
        }

    } catch (const std::exception& e) {
        std::cout << "\n程序异常: " << e.what() << std::endl;
    }

    // 清理
    std::cout << "\n正在清理资源..." << std::endl;
    hand.Disconnect();
    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "✓ 程序已退出" << std::endl;
    return 0;
}
