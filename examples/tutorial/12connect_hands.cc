#include <chrono>
#include <iostream>
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
      << "below with SelectHand() + MoveJoints(), whatever the transport.\n\n"
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

  auto master = ghand::GHand::Create(ghand::ProductType::GHand5, comm_type);
  if (!master) {
    std::cerr << "Failed to create GHand\n";
    return 1;
  }

  // This single call covers every transport and topology listed above: "auto"
  // probes the adapters until one carries a controllable hand, and a named
  // device is used directly. Nothing here knows or cares which transport is
  // in use, or whether a junction is present.
  const std::vector<ghand::ConnectedHand> connected_hands =
      master->Connect(device);
  if (connected_hands.empty()) {
    std::cerr << "Connect failed on " << device << ": "
              << master->GetLastErrorMessage() << "\n\n";
    PrintUsage(argv[0]);
    return 1;
  }

  std::cout << "Connected on " << device << " - " << connected_hands.size()
            << " hand(s):\n";
  for (const auto& hand : connected_hands) {
    std::cout << "  slave 0x" << std::hex << hand.slave_index << std::dec
              << ": " << ghand::ToString(hand.hand_type)
              << " device=" << hand.device_info.device_name
              << " serial=" << hand.device_info.serial_number << '\n';
  }

  // MoveJoints() and every other single-hand API act on whichever hand
  // SelectHand() picked last, so one SelectHand() per hand is all it takes.
  std::cout << "\nMove the hands to a light grasp pose...\n";
  for (const auto& hand : connected_hands) {
    master->SelectHand(hand.slave_index);
    master->MoveJoints(BuildPose(25.0f, 35.0f));
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "Mirror the index/middle finger pose...\n";
  for (const auto& hand : connected_hands) {
    const bool is_left = hand.hand_type == ghand::HandType::LEFT;
    master->SelectHand(hand.slave_index);
    master->MoveJoints(is_left ? BuildPose(45.0f, 60.0f)
                               : BuildPose(10.0f, 15.0f));
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "Reset the hands...\n";
  for (const auto& hand : connected_hands) {
    master->SelectHand(hand.slave_index);
    master->MoveJoints(BuildPose(0.0f, 0.0f));
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));

  for (const auto& hand : connected_hands) {
    if (master->SelectHand(hand.slave_index)) master->Stop();
  }
  master->Disconnect();

  std::cout << "\nDemo completed.\n";
  return 0;
}
