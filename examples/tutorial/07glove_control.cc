// Define these macros before including Windows headers to avoid
// winsock.h/winsock2.h conflicts.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
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

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

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

struct Finger {
  float mcp_bend, mcp_sway;
  float pip_bend, pip_sway;

  Finger() : mcp_bend(0), mcp_sway(0), pip_bend(0), pip_sway(0) {}
};

struct HandData {
  ThumbFinger thumb;
  Finger index;
  Finger middle;
  Finger ring;
  Finger pinky;
};

constexpr char kUdpIp[] = "192.168.1.19";
constexpr int kUdpPort = 8080;
constexpr double kProcessInterval = 0.02;

enum class ReceiveStatus {
  DATA,
  TIMEOUT,
  FAILURE
};

float ClipAngle(float value, float min_angle, float max_angle) {
  float clamped = value;
  if (clamped < min_angle) clamped = min_angle;
  if (clamped > max_angle) clamped = max_angle;
  return clamped;
}

bool ParseFloat(const std::string& text, float* value) {
  if (value == nullptr) return false;
  errno = 0;
  char* end = nullptr;
  float parsed = std::strtof(text.c_str(), &end);
  if (end == text.c_str() || errno == ERANGE) return false;
  *value = parsed;
  return true;
}

std::vector<float> ParseNumericData(const char* data) {
  std::string data_str(data);
  std::vector<float> numeric_data;

  size_t start = 0;
  size_t end = data_str.find(',');
  bool first_item = true;
  while (end != std::string::npos) {
    std::string item = data_str.substr(start, end - start);
    if (!first_item) {
      float value = 0.0f;
      if (ParseFloat(item, &value)) numeric_data.push_back(value);
    } else {
      first_item = false;
    }
    start = end + 1;
    end = data_str.find(',', start);
  }

  if (start < data_str.length() && !first_item) {
    float value = 0.0f;
    if (ParseFloat(data_str.substr(start), &value)) {
      numeric_data.push_back(value);
    }
  }
  return numeric_data;
}

std::vector<float> ExtractHandValues(const std::vector<float>& numeric_data,
                                     const size_t* indices,
                                     size_t count) {
  std::vector<float> values;
  values.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    values.push_back(numeric_data[indices[i]]);
  }
  return values;
}

void FillHandData(const std::vector<float>& values, HandData* hand) {
  if (values.size() < 25) return;

  hand->thumb.mcp_bend = values[0];
  hand->thumb.mcp_sway = values[1];
  hand->thumb.mcp_roll = values[2];
  hand->thumb.pip_bend = values[3];
  hand->thumb.pip_sway = values[4];
  hand->thumb.pip_roll = values[5];
  hand->thumb.dip_bend = values[6];
  hand->thumb.dip_sway = values[7];
  hand->thumb.dip_roll = values[8];

  hand->index.mcp_bend = values[9];
  hand->index.mcp_sway = values[10];
  hand->index.pip_bend = values[11];
  hand->index.pip_sway = values[12];
  hand->middle.mcp_bend = values[13];
  hand->middle.mcp_sway = values[14];
  hand->middle.pip_bend = values[15];
  hand->middle.pip_sway = values[16];
  hand->ring.mcp_bend = values[17];
  hand->ring.mcp_sway = values[18];
  hand->ring.pip_bend = values[19];
  hand->ring.pip_sway = values[20];
  hand->pinky.mcp_bend = values[21];
  hand->pinky.mcp_sway = values[22];
  hand->pinky.pip_bend = values[23];
  hand->pinky.pip_sway = values[24];
}

bool ProcessGloveData(const char* data, HandData& left_hand,
                      HandData& right_hand) {
  const size_t kRightIndices[] = {
      10, 9,  11, 16, 15, 17, 22, 21, 23, 28, 27, 34, 33,
      46, 45, 52, 51, 64, 63, 70, 69, 82, 81, 88, 87};
  const size_t kLeftIndices[] = {
      106, 105, 107, 112, 111, 113, 118, 117, 119,
      124, 123, 130, 129, 142, 141, 148, 147,
      160, 159, 166, 165, 178, 177, 184, 183};

  std::vector<float> numeric_data = ParseNumericData(data);
  if (numeric_data.size() < 192) return false;
  FillHandData(ExtractHandValues(numeric_data, kLeftIndices, 25), &left_hand);
  FillHandData(ExtractHandValues(numeric_data, kRightIndices, 25), &right_hand);
  return true;
}

bool InitializeSocketApi() {
#ifdef _WIN32
  WSADATA wsa_data;
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    std::cerr << "Error: WSAStartup failed" << '\n';
    return false;
  }
#endif
  return true;
}

void CleanupSocketApi() {
#ifdef _WIN32
  WSACleanup();
#endif
}

SOCKET CreateUdpSocket() {
  SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    std::cerr << "Error: Unable to create socket" << '\n';
  }
  return sock;
}

bool BindUdpSocket(SOCKET sock) {
  sockaddr_in server_addr;
  std::memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(kUdpPort);

  if (bind(sock, (sockaddr*)&server_addr, sizeof(server_addr)) ==
      SOCKET_ERROR) {
    std::cerr << "Error: Unable to bind to " << kUdpIp << ":" << kUdpPort
              << '\n';
    return false;
  }

  std::cout << "\nOK Listening for data on " << kUdpIp << ":" << kUdpPort
            << "..." << '\n';
  return true;
}

void ConfigureReceiveTimeout(SOCKET sock) {
#ifdef _WIN32
  DWORD timeout = 1000;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout,
             sizeof(timeout));
#else
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

std::unique_ptr<ghand::DexHand> CreateConnectedHand() {
  auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                      ghand::CommType::CANFD);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << '\n';
    return nullptr;
  }
  std::cout << "\nConnecting to dexterous hand via CANFD..." << '\n';
  if (!hand->AutoConnect()) {
    std::cerr << "Error: Unable to connect to dexterous hand!" << '\n';
    return nullptr;
  }

  std::cout << "OK Successfully connected to dexterous hand!" << '\n';
  return hand;
}

std::vector<ghand::JointCommand> BuildJointCommands(const HandData& hand) {
  const int speed = 100;
  const int torque = 100;
  return {
      {ghand::JointId::THUMB_PIP,
       ClipAngle(hand.thumb.pip_bend, 0, 75), speed, torque},
      {ghand::JointId::THUMB_MCP,
       ClipAngle(hand.thumb.mcp_bend - 40, 0, 55), speed, torque},
      {ghand::JointId::THUMB_SWING,
       ClipAngle(-(hand.thumb.mcp_roll + hand.thumb.pip_roll +
                   hand.thumb.dip_roll) -
                     85,
                 0, 90),
       speed, torque},
      {ghand::JointId::THUMB_ROTATION,
       ClipAngle(-hand.thumb.dip_sway, -30, 60), speed, torque},
      {ghand::JointId::FF_PIP,
       ClipAngle(hand.index.pip_bend, 0, 75), speed, torque},
      {ghand::JointId::FF_MCP,
       ClipAngle(hand.index.mcp_bend, 0, 70), speed, torque},
      {ghand::JointId::FF_SWING,
       ClipAngle(hand.index.mcp_sway + hand.index.pip_sway, -15, 15),
       speed, torque},
      {ghand::JointId::MF_PIP,
       ClipAngle(hand.middle.pip_bend, 0, 75), speed, torque},
      {ghand::JointId::MF_MCP,
       ClipAngle(hand.middle.mcp_bend, 0, 70), speed, torque},
      {ghand::JointId::RF_PIP,
       ClipAngle(hand.ring.pip_bend, 0, 75), speed, torque},
      {ghand::JointId::RF_MCP,
       ClipAngle(hand.ring.mcp_bend, 0, 70), speed, torque},
      {ghand::JointId::LF_PIP,
       ClipAngle(hand.pinky.pip_bend, 0, 75), speed, torque},
      {ghand::JointId::LF_MCP,
       ClipAngle(hand.pinky.mcp_bend, 0, 70), speed, torque},
  };
}

ReceiveStatus ReceivePacket(SOCKET sock, char* buffer, int buffer_size,
                            int* recv_len) {
  sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  *recv_len = recvfrom(sock, buffer, buffer_size - 1, 0,
                       (sockaddr*)&client_addr, &client_len);
  if (*recv_len != SOCKET_ERROR) return ReceiveStatus::DATA;

#ifdef _WIN32
  int error = WSAGetLastError();
  if (error == WSAETIMEDOUT) return ReceiveStatus::TIMEOUT;
#endif
  return ReceiveStatus::FAILURE;
}

void ProcessPacket(char* buffer, int recv_len, ghand::DexHand& hand,
                   int* data_count, bool joints_received,
                   const std::vector<ghand::Joint>& last_joints) {
  buffer[recv_len] = '\0';
  HandData left_hand, right_hand;
  if (!ProcessGloveData(buffer, left_hand, right_hand)) return;

  ++(*data_count);
  if (*data_count % 50 == 0) {
    std::cout << "[Glove Data] Left hand thumb MCP: bend="
              << left_hand.thumb.mcp_bend
              << ", sway=" << left_hand.thumb.mcp_sway
              << ", roll=" << left_hand.thumb.mcp_roll << '\n';
  }

  std::vector<ghand::JointCommand> joints = BuildJointCommands(left_hand);
  hand.MoveJoints(joints);
  if (*data_count % 100 == 0 && joints_received && !last_joints.empty()) {
    std::cout << "[Dexterous Hand Status] Processed " << *data_count
              << " frames" << '\n';
  }
}

void ReceiveLoop(SOCKET sock, ghand::DexHand& hand) {
  std::cout << "\nStarting to receive glove data and control dexterous hand..."
            << '\n';
  std::cout << "Press Ctrl+C to exit program\n" << '\n';

  std::vector<ghand::Joint> last_joints;
  bool joints_received = false;
  hand.SetJointsCallback([&](const std::vector<ghand::Joint>& joints) {
    last_joints = joints;
    joints_received = true;
  });

  auto last_process_time = std::chrono::steady_clock::now();
  int data_count = 0;
  while (true) {
    char buffer[32 * 1024];
    int recv_len = 0;
    ReceiveStatus status =
        ReceivePacket(sock, buffer, sizeof(buffer), &recv_len);
    if (status == ReceiveStatus::TIMEOUT) continue;
    if (status == ReceiveStatus::FAILURE) {
      std::cerr << "Error: Failed to receive data" << '\n';
      break;
    }

    auto current_time = std::chrono::steady_clock::now();
    double elapsed =
        std::chrono::duration<double>(current_time - last_process_time)
            .count();
    if (elapsed >= kProcessInterval) {
      ProcessPacket(buffer, recv_len, hand, &data_count, joints_received,
                    last_joints);
      last_process_time = current_time;
    }
  }
}

int main() {
  std::cout << "========================================" << '\n';
  std::cout << "  GHand Dexterous Hand SDK - Glove Control        "
            << '\n';
  std::cout << "========================================" << '\n';

  if (!InitializeSocketApi()) return 1;

  SOCKET sock = CreateUdpSocket();
  if (sock == INVALID_SOCKET) {
    CleanupSocketApi();
    return 1;
  }

  if (!BindUdpSocket(sock)) {
    closesocket(sock);
    CleanupSocketApi();
    return 1;
  }

  ConfigureReceiveTimeout(sock);
  auto hand = CreateConnectedHand();
  if (!hand) {
    closesocket(sock);
    CleanupSocketApi();
    return 1;
  }

  ReceiveLoop(sock, *hand);

  std::cout << "\nCleaning up resources..." << '\n';
  hand->Disconnect();
  closesocket(sock);
  CleanupSocketApi();

  std::cout << "OK Program exited" << '\n';
  return 0;
}
