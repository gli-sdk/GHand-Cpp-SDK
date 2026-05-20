// 在包含 Windows 头文件之前先定义这些宏，避免 winsock.h/winsock2.h 冲突
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#define closesocket close
typedef int SOCKET;
const int INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;
#endif

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

#include "ghand/ghand.h"

using namespace ghand;

// ========== Data Structures ==========

/**
 * @brief Thumb finger data (9 sensors)
 */
struct ThumbFinger {
  float mcp_bend, mcp_sway, mcp_roll;
  float pip_bend, pip_sway, pip_roll;
  float dip_bend, dip_sway, dip_roll;

  ThumbFinger()
      : mcp_bend(0),
        mcp_sway(0),
        mcp_roll(0),
        pip_bend(0),
        pip_sway(0),
        pip_roll(0),
        dip_bend(0),
        dip_sway(0),
        dip_roll(0) {}
};

/**
 * @brief Other finger data (4 sensors)
 */
struct Finger {
  float mcp_bend, mcp_sway;
  float pip_bend, pip_sway;

  Finger() : mcp_bend(0), mcp_sway(0), pip_bend(0), pip_sway(0) {}
};

/**
 * @brief Hand data
 */
struct HandData {
  ThumbFinger thumb;
  Finger index;
  Finger middle;
  Finger ring;
  Finger pinky;
};

// ========== Configuration Parameters ==========

constexpr char kUdpIp[] = "192.168.1.19";
constexpr int kUdpPort = 8080;
constexpr double kProcessInterval = 0.02;  // 20ms processing interval

// ========== Helper Functions ==========

/**
 * @brief Angle clipping function (clips angle to specified range)
 * @param value Input angle value (degrees)
 * @param min_angle Minimum angle value (degrees)
 * @param max_angle Maximum angle value (degrees)
 * @return Angle value clipped within range (degrees)
 */
float ClipAngle(float value, float min_angle, float max_angle) {
  float clamped = value;
  if (clamped < min_angle) clamped = min_angle;
  if (clamped > max_angle) clamped = max_angle;
  return clamped;
}

/**
 * @brief Process glove data, parse left and right hand finger data
 * @param data Raw data string
 * @param left_hand Output: left hand data
 * @param right_hand Output: right hand data
 * @return Returns true on success
 */
bool ProcessGloveData(const char* data, HandData& left_hand,
                      HandData& right_hand) {
  // Copy data to avoid modifying original string
  std::string data_str(data);
  std::vector<float> numeric_data;

  // Manually parse comma-separated data
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
        // Conversion failed, skip
      }
    } else {
      first_item = false;
    }

    start = end + 1;
    end = data_str.find(',', start);
  }

  // Process last item
  if (start < data_str.length() && !first_item) {
    try {
      float value = std::stof(data_str.substr(start));
      numeric_data.push_back(value);
    } catch (const std::exception&) {
      // Conversion failed, skip
    }
  }

  // Need at least 192 data points
  if (numeric_data.size() < 192) {
    return false;
  }

  // Extract right hand data
  std::vector<float> right_hand_data;
  // Right hand thumb (groups 1, 2, 3)
  right_hand_data.push_back(numeric_data[10]);
  right_hand_data.push_back(numeric_data[9]);
  right_hand_data.push_back(numeric_data[11]);  // mcp bend, sway, roll
  right_hand_data.push_back(numeric_data[16]);
  right_hand_data.push_back(numeric_data[15]);
  right_hand_data.push_back(numeric_data[17]);  // pip bend, sway, roll
  right_hand_data.push_back(numeric_data[22]);
  right_hand_data.push_back(numeric_data[21]);
  right_hand_data.push_back(numeric_data[23]);  // dip bend, sway, roll

  // Right hand index finger (groups 4, 5)
  right_hand_data.push_back(numeric_data[28]);
  right_hand_data.push_back(numeric_data[27]);  // mcp
  right_hand_data.push_back(numeric_data[34]);
  right_hand_data.push_back(numeric_data[33]);  // pip

  // Right hand middle finger (groups 7, 8)
  right_hand_data.push_back(numeric_data[46]);
  right_hand_data.push_back(numeric_data[45]);  // mcp
  right_hand_data.push_back(numeric_data[52]);
  right_hand_data.push_back(numeric_data[51]);  // pip

  // Right hand ring finger (groups 10, 11)
  right_hand_data.push_back(numeric_data[64]);
  right_hand_data.push_back(numeric_data[63]);  // mcp
  right_hand_data.push_back(numeric_data[70]);
  right_hand_data.push_back(numeric_data[69]);  // pip

  // Right hand little finger (groups 13, 14)
  right_hand_data.push_back(numeric_data[82]);
  right_hand_data.push_back(numeric_data[81]);  // mcp
  right_hand_data.push_back(numeric_data[88]);
  right_hand_data.push_back(numeric_data[87]);  // pip

  // Extract left hand data
  std::vector<float> left_hand_data;
  // Left hand thumb (groups 17, 18, 19)
  left_hand_data.push_back(numeric_data[106]);
  left_hand_data.push_back(numeric_data[105]);
  left_hand_data.push_back(numeric_data[107]);  // mcp bend, sway, roll
  left_hand_data.push_back(numeric_data[112]);
  left_hand_data.push_back(numeric_data[111]);
  left_hand_data.push_back(numeric_data[113]);  // pip bend, sway, roll
  left_hand_data.push_back(numeric_data[118]);
  left_hand_data.push_back(numeric_data[117]);
  left_hand_data.push_back(numeric_data[119]);  // dip bend, sway, roll

  // Left hand index finger (groups 20, 21)
  left_hand_data.push_back(numeric_data[124]);
  left_hand_data.push_back(numeric_data[123]);  // mcp
  left_hand_data.push_back(numeric_data[130]);
  left_hand_data.push_back(numeric_data[129]);  // pip

  // Left hand middle finger (groups 23, 24)
  left_hand_data.push_back(numeric_data[142]);
  left_hand_data.push_back(numeric_data[141]);  // mcp
  left_hand_data.push_back(numeric_data[148]);
  left_hand_data.push_back(numeric_data[147]);  // pip

  // Left hand ring finger (groups 26, 27)
  left_hand_data.push_back(numeric_data[160]);
  left_hand_data.push_back(numeric_data[159]);  // mcp
  left_hand_data.push_back(numeric_data[166]);
  left_hand_data.push_back(numeric_data[165]);  // pip

  // Left hand little finger (groups 29, 30)
  left_hand_data.push_back(numeric_data[178]);
  left_hand_data.push_back(numeric_data[177]);  // mcp
  left_hand_data.push_back(numeric_data[184]);
  left_hand_data.push_back(numeric_data[183]);  // pip

  // Build right hand data object
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

  // Build left hand data object
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

// ========== Main Program ==========

int main() {
  std::cout << "========================================" << std::endl;
  std::cout << "  GHand Dexterous Hand SDK - Glove Control        "
            << std::endl;
  std::cout << "========================================" << std::endl;

#ifdef _WIN32
  // Initialize Winsock
  WSADATA wsa_data;
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    std::cerr << "Error: WSAStartup failed" << std::endl;
    return 1;
  }
#endif

  // Create UDP socket
  SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    std::cerr << "Error: Unable to create socket" << std::endl;
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }

  // Bind socket
  sockaddr_in server_addr;
  std::memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(kUdpPort);

  if (bind(sock, (sockaddr*)&server_addr, sizeof(server_addr)) ==
      SOCKET_ERROR) {
    std::cerr << "Error: Unable to bind to " << kUdpIp << ":" << kUdpPort
              << std::endl;
    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }

  std::cout << "\n✓ Listening for data on " << kUdpIp << ":" << kUdpPort
            << "..." << std::endl;

  // Connect dexterous hand
  auto hand = DexHand::Create(ProductType::G5, CommType::ETHERCAT);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << std::endl;
    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }
  std::cout << "\nConnecting to dexterous hand via EtherCAT..." << std::endl;
  bool success = hand->AutoConnect();

  if (!success) {
    std::cerr << "Error: Unable to connect to dexterous hand!" << std::endl;
    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }

  std::cout << "✓ Successfully connected to dexterous hand!" << std::endl;
  std::cout << "\nStarting to receive glove data and control dexterous hand..."
            << std::endl;
  std::cout << "Press Ctrl+C to exit program\n" << std::endl;

  // Set receive timeout to avoid permanent blocking
#ifdef _WIN32
  DWORD timeout = 1000;  // 1 second timeout
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout,
             sizeof(timeout));
#else
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

  // Register joint data callback (for displaying feedback)
  std::vector<Joint> last_joints;
  bool joints_received = false;
  hand->SetJointsCallback([&](const std::vector<Joint>& joints) {
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
      double elapsed =
          std::chrono::duration<double>(current_time - last_process_time)
              .count();

      if (elapsed >= kProcessInterval) {
        HandData left_hand, right_hand;

        if (ProcessGloveData(buffer, left_hand, right_hand)) {
          data_count++;

          // Display glove data every 50 frames
          if (data_count % 50 == 0) {
            std::cout << "[Glove Data] Left hand thumb MCP: bend="
                      << left_hand.thumb.mcp_bend
                      << ", sway=" << left_hand.thumb.mcp_sway
                      << ", roll=" << left_hand.thumb.mcp_roll << std::endl;
          }

          // Use left hand data to control dexterous hand
          std::vector<JointCommand> joints;
          const int speed = 100;
          const int torque = 100;

          // Thumb joints
          joints.push_back({JointId::THUMB_PIP,
                            ClipAngle(left_hand.thumb.pip_bend, 0, 75), speed,
                            torque});
          joints.push_back({JointId::THUMB_MCP,
                            ClipAngle(left_hand.thumb.mcp_bend - 40, 0, 55),
                            speed, torque});
          joints.push_back(
              {JointId::THUMB_SWING,
               ClipAngle(-(left_hand.thumb.mcp_roll + left_hand.thumb.pip_roll +
                           left_hand.thumb.dip_roll) -
                             85,
                         0, 90),
               speed, torque});
          joints.push_back({JointId::THUMB_ROTATION,
                            ClipAngle(-left_hand.thumb.dip_sway, -30, 60),
                            speed, torque});

          // Index finger joints
          joints.push_back({JointId::FF_PIP,
                            ClipAngle(left_hand.index.pip_bend, 0, 75), speed,
                            torque});
          joints.push_back({JointId::FF_MCP,
                            ClipAngle(left_hand.index.mcp_bend, 0, 70), speed,
                            torque});
          joints.push_back(
              {JointId::FF_SWING,
               ClipAngle(left_hand.index.mcp_sway + left_hand.index.pip_sway,
                         -15, 15),
               speed, torque});

          // Middle finger joints
          joints.push_back({JointId::MF_PIP,
                            ClipAngle(left_hand.middle.pip_bend, 0, 75), speed,
                            torque});
          joints.push_back({JointId::MF_MCP,
                            ClipAngle(left_hand.middle.mcp_bend, 0, 70), speed,
                            torque});

          // Ring finger joints
          joints.push_back({JointId::RF_PIP,
                            ClipAngle(left_hand.ring.pip_bend, 0, 75), speed,
                            torque});
          joints.push_back({JointId::RF_MCP,
                            ClipAngle(left_hand.ring.mcp_bend, 0, 70), speed,
                            torque});

          // Little finger joints
          joints.push_back({JointId::LF_PIP,
                            ClipAngle(left_hand.pinky.pip_bend, 0, 75), speed,
                            torque});
          joints.push_back({JointId::LF_MCP,
                            ClipAngle(left_hand.pinky.mcp_bend, 0, 70), speed,
                            torque});

          // Send joint commands
          hand->MoveJoints(joints);

          // Display joint status every 100 frames
          if (data_count % 100 == 0 && joints_received &&
              !last_joints.empty()) {
            std::cout << "[Dexterous Hand Status] Processed " << data_count
                      << " frames" << std::endl;
          }
        }

        last_process_time = current_time;
      }
    }

  } catch (const std::exception& e) {
    std::cout << "\nProgram exception: " << e.what() << std::endl;
  }

  // Cleanup
  std::cout << "\nCleaning up resources..." << std::endl;
  hand->Disconnect();
  closesocket(sock);
#ifdef _WIN32
  WSACleanup();
#endif

  std::cout << "✓ Program exited" << std::endl;
  return 0;
}
