// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

namespace {

void PrintUsage(const char* program) {
  std::cout
      << "Usage:\n"
      << "  " << program << " [ethcat|canfd|rs485] [device]\n\n"
      << "Examples:\n"
      << "  " << program << "                    (EtherCAT, auto adapter)\n"
      << "  " << program << " ethcat <adapter>\n"
      << "  " << program << " canfd COM17:0\n"
      << "  " << program << " rs485 /dev/ttyUSB0\n\n"
      << "Without a device every adapter is probed and the first bus that\n"
      << "carries a hand is used. Every hand found is driven through the poses\n"
      << "below with MoveJoints(), whatever the transport: calling it on the\n"
      << "session drives every hand, calling it on one hand drives only that\n"
      << "hand.\n\n"
      << "EtherCAT topologies all handled by the same code below:\n"
      << "  no junction,   single hand : PC -> hand\n"
      << "  no junction,   dual hand   : PC -> {handA, handB}\n"
      << "  with junction, single hand : PC -> junction -> hand\n"
      << "  with junction, dual hand   : PC -> junction -> {handA, handB}\n"
      << "The EtherCAT junction is a slave too, but it is not a hand, so\n"
      << "Connect() skips it. On CAN-FD and RS485 both hands share one bus\n"
      << "(1-in 2-out) and Connect() scans the default addresses 0x31/0x32.\n";
}

std::vector<ghand::JointCommand> BuildPose(float mcp, float pip) {
  return {
      {ghand::JointId::FF_MCP, mcp, 80, 80},
      {ghand::JointId::FF_PIP, pip, 80, 80},
      {ghand::JointId::MF_MCP, mcp, 80, 80},
      {ghand::JointId::MF_PIP, pip, 80, 80},
  };
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc > 1) {
    const std::string arg = argv[1];
    if (arg == "-h" || arg == "--help") {
      PrintUsage(argv[0]);
      return 0;
    }
  }

  ghand::CommType comm_type = ghand::CommType::ETHERCAT;
  int first_device_arg = 1;
  if (argc > 1) {
    const std::string arg = argv[1];
    if (arg == "ethcat") {
      first_device_arg = 2;
    } else if (arg == "canfd") {
      comm_type = ghand::CommType::CANFD;
      first_device_arg = 2;
    } else if (arg == "rs485") {
      comm_type = ghand::CommType::RS485;
      first_device_arg = 2;
    } else if (!arg.empty() && arg.front() == '-') {
      PrintUsage(argv[0]);
      return 1;
    }
  }

  const std::string device =
      argc > first_device_arg ? argv[first_device_arg] : "auto";

  auto master = ghand::Master::Create(ghand::ProductType::GHand5, comm_type);
  if (!master) {
    std::cerr << "Failed to create GHand\n";
    return 1;
  }

  // This single call covers every transport and topology listed above: "auto"
  // probes the adapters until one carries a controllable hand, and a named
  // device is used directly. Nothing here knows or cares which transport is
  // in use, or whether a junction is present.
  const auto connected = master->Connect(device);
  if (!connected) {
    std::cerr << "Connect failed on " << device << ": " << connected.message
              << "\n\n";
    PrintUsage(argv[0]);
    return 1;
  }
  const std::vector<ghand::GHand*>& hands = connected.value;

  std::cout << "Connected on " << device << " - " << hands.size()
            << " hand(s):\n";
  for (ghand::GHand* hand : hands) {
    const ghand::DeviceInfo info = hand->device_info();
    std::cout << "  slave 0x" << std::hex << hand->slave_index() << std::dec
              << ": " << ghand::ToString(hand->hand_type())
              << " device=" << info.device_name
              << " serial=" << info.serial_number << '\n';
  }

  // Each callback is registered on one hand and only receives that hand's
  // data, so the latest joints are kept per hand instead of in one shared
  // variable.
  std::mutex state_mutex;
  std::map<uint16_t, std::vector<ghand::Joint>> joints_by_hand;
  for (ghand::GHand* hand : hands) {
    hand->SetJointsCallback(
        [&state_mutex, &joints_by_hand, hand](
            const std::vector<ghand::Joint>& joints) {
          std::lock_guard<std::mutex> lock(state_mutex);
          joints_by_hand[hand->slave_index()] = joints;
        });
  }

  auto print_feedback = [&]() {
    std::lock_guard<std::mutex> lock(state_mutex);
    for (ghand::GHand* hand : hands) {
      const auto it = joints_by_hand.find(hand->slave_index());
      if (it == joints_by_hand.end() || it->second.empty()) {
        std::cout << "  slave " << hand->slave_index() << ": no feedback yet\n";
        continue;
      }
      const ghand::Joint& first = it->second.front();
      std::cout << "  slave " << hand->slave_index() << ": "
                << it->second.size() << " joints, "
                << ghand::ToString(first.id) << " = " << first.angle
                << " deg\n";
    }
  };

  bool failed = false;

  // A session-level call drives every hand at once, so a shared pose needs no
  // loop at all.
  std::cout << "\nMove the hands to a light grasp pose...\n";
  const auto grasp_result = master->MoveJoints(BuildPose(25.0f, 35.0f));
  if (!grasp_result) {
    std::cerr << "Grasp command failed: " << grasp_result.message << '\n';
    failed = true;
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  print_feedback();

  // A pose that differs per hand is sent hand by hand.
  std::cout << "Mirror the index/middle finger pose...\n";
  for (ghand::GHand* hand : hands) {
    const bool is_left = hand->hand_type() == ghand::HandType::LEFT;
    const auto result = hand->MoveJoints(
        is_left ? BuildPose(45.0f, 60.0f) : BuildPose(10.0f, 15.0f));
    if (!result) {
      std::cerr << "Mirror command failed on slave 0x" << std::hex
                << hand->slave_index() << std::dec << ": " << result.message
                << '\n';
      failed = true;
    }
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  print_feedback();

  std::cout << "Reset the hands...\n";
  const auto reset_result = master->MoveJoints(BuildPose(0.0f, 0.0f));
  if (!reset_result) {
    std::cerr << "Reset command failed: " << reset_result.message << '\n';
    failed = true;
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  print_feedback();

  const auto stop_result = master->Stop();
  if (!stop_result) {
    std::cerr << "Stop failed: " << stop_result.message << '\n';
    failed = true;
  }
  const auto disconnected = master->Disconnect();
  if (!disconnected) {
    std::cerr << "Disconnect failed: " << disconnected.message << '\n';
    return 1;
  }

  std::cout << "\nDemo completed.\n";
  return failed ? 1 : 0;
}
