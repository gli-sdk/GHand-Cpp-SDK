#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

namespace {

void PrintUsage(const char* program) {
  std::cout << "Usage:\n"
            << "  " << program << " [adapter] [left_slave] [right_slave]\n\n"
            << "Examples:\n"
            << "  " << program << "                 (auto adapter, auto left/right)\n"
            << "  " << program << " enp4s0          (fixed adapter, auto left/right)\n"
            << "  " << program << " enp4s0 3 2      (fixed adapter and slaves)\n\n"
            << "The PC runs as one EtherCAT master on the selected adapter.\n"
            << "Both dexterous hands are EtherCAT slaves on the same bus.\n"
            << "An EtherCAT junction may sit in front of them; it is a slave too\n"
            << "and is skipped automatically. Without an explicit slave number the\n"
            << "left/right hands are identified by their reported hand type.\n";
}

std::vector<ghand::JointCommand> BuildPose(float mcp, float pip) {
  return {
      {ghand::JointId::FF_MCP, mcp, 80, 80},
      {ghand::JointId::FF_PIP, pip, 80, 80},
      {ghand::JointId::MF_MCP, mcp, 80, 80},
      {ghand::JointId::MF_PIP, pip, 80, 80},
  };
}

// EtherCAT slave indices start at 1.
bool ParseSlave(const char* text, uint16_t* slave) {
  if (text == nullptr || *text == '\0') return false;
  char* end = nullptr;
  long value = std::strtol(text, &end, 10);
  if (end == text || *end != '\0' || value < 1 || value > 65535) return false;
  *slave = static_cast<uint16_t>(value);
  return true;
}

bool MarkAdapterBase(std::set<std::string>* seen, const std::string& name) {
  const std::string base = name.substr(0, name.find(':'));
  return seen->insert(base).second;
}

uint16_t FindHand(const std::vector<ghand::ConnectedHand>& hands,
                  ghand::HandType hand_type) {
  uint16_t slave_index = 0;
  for (const auto& hand : hands) {
    if (hand.hand_type != hand_type) continue;
    if (slave_index != 0) return 0;
    slave_index = hand.slave_index;
  }
  return slave_index;
}

bool HasSlave(const std::vector<ghand::ConnectedHand>& hands,
              uint16_t slave_index) {
  for (const auto& hand : hands) {
    if (hand.slave_index == slave_index) return true;
  }
  return false;
}

void PrintHands(const std::vector<ghand::ConnectedHand>& hands) {
  if (hands.empty()) {
    std::cout << "  (no hand detected on this bus)\n";
    return;
  }
  for (const auto& hand : hands) {
    std::cout << "  slave " << hand.slave_index << ": "
              << ghand::ToString(hand.hand_type)
              << " device=" << hand.device_info.device_name
              << " serial=" << hand.device_info.serial_number << '\n';
  }
}

// Connect one adapter and keep it only when the bus actually carries a hand.
std::unique_ptr<ghand::GHand> OpenBus(const std::string& adapter,
                                      std::vector<ghand::ConnectedHand>* hands) {
  std::cout << "Connecting EtherCAT bus on " << adapter << "...\n";
  auto master = ghand::GHand::Create(ghand::ProductType::GHand5,
                                     ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create EtherCAT master\n";
    return nullptr;
  }
  if (!master->Connect(adapter)) {
    std::cout << "  No EtherCAT bus: " << master->GetLastErrorMessage() << '\n';
    return nullptr;
  }
  *hands = master->GetConnectedHands();
  if (hands->empty()) {
    std::cout << "  Bus has no hand on it\n";
    master->Disconnect();
    return nullptr;
  }
  return master;
}

// Probe every adapter and return the first bus that carries a hand.
std::unique_ptr<ghand::GHand> OpenFirstBus(
    std::vector<ghand::ConnectedHand>* hands, std::string* adapter) {
  auto probe = ghand::GHand::Create(ghand::ProductType::GHand5,
                                    ghand::CommType::ETHERCAT);
  if (!probe) {
    std::cerr << "Failed to create EtherCAT master\n";
    return nullptr;
  }

  std::map<std::string, std::string> adapters = probe->SearchAdapters();
  std::set<std::string> seen_bases;
  for (const auto& entry : adapters) {
    if (!MarkAdapterBase(&seen_bases, entry.first)) continue;
    auto master = OpenBus(entry.first, hands);
    if (master) {
      *adapter = entry.first;
      return master;
    }
  }
  return nullptr;
}

void MoveBoth(ghand::GHand* master, uint16_t left_slave, uint16_t right_slave,
              const std::vector<ghand::JointCommand>& left_pose,
              const std::vector<ghand::JointCommand>& right_pose) {
  if (!master->SelectHand(left_slave) || !master->MoveJoints(left_pose)) {
    std::cerr << "Failed to move slave " << left_slave << ": "
              << master->GetLastErrorMessage() << '\n';
  }
  if (!master->SelectHand(right_slave) || !master->MoveJoints(right_pose)) {
    std::cerr << "Failed to move slave " << right_slave << ": "
              << master->GetLastErrorMessage() << '\n';
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc > 1) {
    std::string arg = argv[1];
    if (arg == "-h" || arg == "--help") {
      PrintUsage(argv[0]);
      return 0;
    }
  }

  const bool adapter_given = argc > 1;
  std::string adapter = adapter_given ? argv[1] : "";

  uint16_t left_slave = 0;
  uint16_t right_slave = 0;
  const bool slaves_given =
      argc > 3 && ParseSlave(argv[2], &left_slave) &&
      ParseSlave(argv[3], &right_slave);

  std::cout << "GHand EtherCAT master-slave dual-hand demo\n";
  std::cout << "Master: this process / SOEM\n";

  std::vector<ghand::ConnectedHand> hands;
  std::unique_ptr<ghand::GHand> master =
      adapter_given ? OpenBus(adapter, &hands) : OpenFirstBus(&hands, &adapter);
  if (!master) {
    std::cerr << "\nNo hand found on "
              << (adapter_given ? adapter : std::string("any adapter"))
              << ".\n\n";
    PrintUsage(argv[0]);
    return 1;
  }

  std::cout << "Adapter: " << adapter << '\n';
  std::cout << "Slaves on the bus:\n";
  PrintHands(hands);

  if (!slaves_given) {
    // No explicit slave numbers: identify the hands by their reported type.
    left_slave = FindHand(hands, ghand::HandType::LEFT);
    right_slave = FindHand(hands, ghand::HandType::RIGHT);
    if (left_slave == 0 || right_slave == 0) {
      std::cerr << "Expected exactly one left hand and one right hand; pass "
                   "explicit slave numbers instead:\n";
      PrintUsage(argv[0]);
      master->Disconnect();
      return 1;
    }
    std::cout << "Auto-detected slaves - left: " << left_slave
              << ", right: " << right_slave << '\n';
  } else {
    std::cout << "Slaves from command line - left: " << left_slave
              << ", right: " << right_slave << '\n';
    if (!HasSlave(hands, left_slave) || !HasSlave(hands, right_slave)) {
      std::cout << "Warning: one of the given slaves did not report as a hand\n";
    }
  }

  std::cout << "\nMove both hands to a light grasp pose...\n";
  MoveBoth(master.get(), left_slave, right_slave, BuildPose(25.0f, 35.0f),
           BuildPose(25.0f, 35.0f));
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "Mirror the index/middle finger pose...\n";
  MoveBoth(master.get(), left_slave, right_slave, BuildPose(45.0f, 60.0f),
           BuildPose(10.0f, 15.0f));
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "Reset both hands...\n";
  MoveBoth(master.get(), left_slave, right_slave, BuildPose(0.0f, 0.0f),
           BuildPose(0.0f, 0.0f));
  std::this_thread::sleep_for(std::chrono::seconds(1));

  if (master->SelectHand(right_slave)) master->Stop();
  if (master->SelectHand(left_slave)) master->Stop();
  master->Disconnect();

  std::cout << "Demo completed.\n";
  return 0;
}
