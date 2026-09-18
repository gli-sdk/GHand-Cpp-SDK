#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

namespace {

// One EtherCAT bus: a single adapter driven by a single master object.
struct Bus {
  std::unique_ptr<ghand::GHand> master;
  std::string adapter;
  std::vector<ghand::ConnectedHand> hands;
};

void PrintUsage(const char* program) {
  std::cout << "Usage:\n"
            << "  " << program << " [adapter]\n\n"
            << "Without arguments every adapter is probed and every hand found on\n"
            << "any bus is detected automatically. Pass an adapter name to probe\n"
            << "only that one (faster, and useful when another NIC is busy).\n\n"
            << "Supported topologies:\n"
            << "  no junction,  single hand : PC -> hand            hand = slave1\n"
            << "  no junction,  dual hand   : PC -> handA, handB    each = slave1\n"
            << "  with junction, single hand: PC -> junction -> hand\n"
            << "  with junction, dual hand  : PC -> junction -> {handA, handB}\n"
            << "The EtherCAT junction is itself a slave and is skipped here.\n";
}

std::vector<ghand::JointCommand> BuildPose(float mcp, float pip) {
  return {
      {ghand::JointId::FF_MCP, mcp, 80, 80},
      {ghand::JointId::FF_PIP, pip, 80, 80},
      {ghand::JointId::MF_MCP, mcp, 80, 80},
      {ghand::JointId::MF_PIP, pip, 80, 80},
  };
}

std::map<std::string, std::string> SearchAdapters() {
  auto probe = ghand::GHand::Create(ghand::ProductType::GHand5,
                                    ghand::CommType::ETHERCAT);
  if (!probe) {
    std::cerr << "Failed to create GHand for adapter discovery\n";
    return {};
  }
  return probe->SearchAdapters();
}

// Deduplicate Windows multi-channel adapters (one NIC is reported once per
// channel, e.g. "\Device\NPF_{GUID}:0" and "\Device\NPF_{GUID}:1").
bool MarkAdapterBase(std::set<std::string>* seen, const std::string& name) {
  const std::string base = name.substr(0, name.find(':'));
  return seen->insert(base).second;
}

// Probe one adapter: connect the bus, then enumerate every slave on it.
// Returns false when the adapter carries no hand, so the caller can drop it.
bool ProbeBus(const std::string& adapter, Bus* bus) {
  std::cout << "Probing adapter: " << adapter << '\n';

  auto master = ghand::GHand::Create(ghand::ProductType::GHand5,
                                     ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "  Failed to create EtherCAT master\n";
    return false;
  }
  if (!master->Connect(adapter)) {
    std::cout << "  No EtherCAT bus here: " << master->GetLastErrorMessage()
              << '\n';
    return false;
  }

  // A single connect scans the whole chain and returns every slave at once;
  // slaves reporting a non-hand device name (the junction) are filtered out.
  std::vector<ghand::ConnectedHand> hands = master->GetConnectedHands();
  if (hands.empty()) {
    std::cout << "  Bus has no hand on it\n";
    master->Disconnect();
    return false;
  }

  bus->master = std::move(master);
  bus->adapter = adapter;
  bus->hands = std::move(hands);
  return true;
}

void PrintTopology(const std::vector<Bus>& buses) {
  size_t total_hands = 0;
  std::cout << "\n========== Detected topology ==========\n";
  for (const auto& bus : buses) {
    std::cout << "Adapter " << bus.adapter << " (" << bus.hands.size()
              << " hand(s)):\n";
    for (const auto& hand : bus.hands) {
      total_hands++;
      std::cout << "  slave " << hand.slave_index << ": "
                << ghand::ToString(hand.hand_type)
                << " device=" << hand.device_info.device_name
                << " serial=" << hand.device_info.serial_number << '\n';
    }
  }
  std::cout << "Total: " << buses.size() << " bus(es), " << total_hands
            << " hand(s)\n";
}

bool MoveAll(const std::vector<Bus>& buses,
             const std::vector<ghand::JointCommand>& pose,
             const std::string& label) {
  std::cout << "\n" << label << '\n';
  bool ok = true;
  for (const auto& bus : buses) {
    for (const auto& hand : bus.hands) {
      if (!bus.master->SelectHand(hand.slave_index) ||
          !bus.master->MoveJoints(pose)) {
        std::cerr << "  Failed on " << bus.adapter << " slave "
                  << hand.slave_index << ": "
                  << bus.master->GetLastErrorMessage() << '\n';
        ok = false;
      }
    }
  }
  return ok;
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

  std::cout << "GHand EtherCAT auto-detect demo\n";
  std::cout << "Master: this process / SOEM\n";

  std::vector<std::string> adapters;
  if (argc > 1) {
    adapters.push_back(argv[1]);
    std::cout << "Adapter (from command line): " << argv[1] << '\n';
  } else {
    std::map<std::string, std::string> found = SearchAdapters();
    std::set<std::string> seen_bases;
    for (const auto& adapter : found) {
      if (MarkAdapterBase(&seen_bases, adapter.first)) {
        adapters.push_back(adapter.first);
      }
    }
    std::cout << "Adapters to probe: " << adapters.size() << '\n';
  }

  if (adapters.empty()) {
    std::cerr << "No network adapter available.\n\n";
    PrintUsage(argv[0]);
    return 1;
  }

  std::cout << "\nConnecting every adapter...\n";
  std::vector<Bus> buses;
  for (const std::string& adapter : adapters) {
    Bus bus;
    if (ProbeBus(adapter, &bus)) {
      std::cout << "  Connected: " << adapter << '\n';
      buses.push_back(std::move(bus));
    }
  }

  if (buses.empty()) {
    std::cerr << "\nNo hand found on any adapter.\n";
    PrintUsage(argv[0]);
    return 1;
  }

  PrintTopology(buses);

  MoveAll(buses, BuildPose(25.0f, 35.0f), "Moving all hands to a light grasp pose...");
  std::this_thread::sleep_for(std::chrono::seconds(2));

  MoveAll(buses, BuildPose(45.0f, 60.0f), "Closing all hands...");
  std::this_thread::sleep_for(std::chrono::seconds(2));

  MoveAll(buses, BuildPose(0.0f, 0.0f), "Resetting all hands...");
  std::this_thread::sleep_for(std::chrono::seconds(1));

  for (const auto& bus : buses) {
    for (const auto& hand : bus.hands) {
      if (bus.master->SelectHand(hand.slave_index)) bus.master->Stop();
    }
    bus.master->Disconnect();
  }

  std::cout << "\nDemo completed.\n";
  return 0;
}
