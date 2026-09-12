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
            << "  " << program << " enp4s0\n"
            << "  " << program << " enp4s0 1 2\n\n"
            << "The PC runs as one EtherCAT master on the selected adapter.\n"
            << "Both dexterous hands are EtherCAT slaves on the same bus.\n";
}

std::vector<ghand::JointCommand> BuildPose(float mcp, float pip) {
  return {
      {ghand::JointId::FF_MCP, mcp, 80, 80},
      {ghand::JointId::FF_PIP, pip, 80, 80},
      {ghand::JointId::MF_MCP, mcp, 80, 80},
      {ghand::JointId::MF_PIP, pip, 80, 80},
  };
}

bool IsLikelyEthercatAdapter(const std::string& name) {
  if (name.empty()) return false;
  if (name == "lo") return false;
  if (name.find("Loopback") != std::string::npos) return false;
  if (name.find("Mihomo") != std::string::npos) return false;
  if (name.find("docker") == 0 || name.find("br-") == 0 ||
      name.find("veth") == 0 || name.find("virbr") == 0 ||
      name.find("vmnet") == 0) {
    return false;
  }
  if (name.find("wl") == 0 || name.find("wifi") != std::string::npos ||
      name.find("wlan") == 0) {
    return false;
  }
  return name.find("en") == 0 || name.find("eth") == 0 ||
         name.find("eno") == 0 || name.find("ens") == 0 ||
         name.find("enp") == 0 || name.find("enx") == 0;
}

std::string SearchFirstEthercatAdapter() {
  auto hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                   ghand::CommType::ETHERCAT);
  if (!hand) return "";

  std::map<std::string, std::string> adapters = hand->SearchAdapters();
  std::set<std::string> adapter_bases;
  for (const auto& adapter : adapters) {
    const std::string& name = adapter.first;
    std::string base = name.substr(0, name.find(':'));
    bool likely_ethercat = IsLikelyEthercatAdapter(base);
    std::cout << (likely_ethercat ? "Candidate adapter: "
                                  : "Skipping adapter: ")
              << name << " - " << adapter.second << '\n';
    if (!likely_ethercat) continue;
    if (adapter_bases.insert(base).second) return name;
  }
  return "";
}

uint16_t ParseSlaveIndex(const char* text, uint16_t fallback) {
  if (text == nullptr) return fallback;
  char* end = nullptr;
  long value = std::strtol(text, &end, 0);
  if (end == text || *end != '\0' || value <= 0 || value > 65535) {
    return fallback;
  }
  return static_cast<uint16_t>(value);
}

void MoveBoth(ghand::GHand* master, uint16_t left_slave, uint16_t right_slave,
              const std::vector<ghand::JointCommand>& left_pose,
              const std::vector<ghand::JointCommand>& right_pose) {
  if (!master->MoveJointsToSlave(left_slave, left_pose)) {
    std::cerr << "Failed to move slave " << left_slave << ": "
              << master->GetLastErrorMessage() << '\n';
  }
  if (!master->MoveJointsToSlave(right_slave, right_pose)) {
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

  std::string adapter = argc > 1 ? argv[1] : SearchFirstEthercatAdapter();
  uint16_t left_slave = ParseSlaveIndex(argc > 2 ? argv[2] : nullptr, 1);
  uint16_t right_slave = ParseSlaveIndex(argc > 3 ? argv[3] : nullptr, 2);

  if (adapter.empty()) {
    std::cerr << "No EtherCAT wired adapter found.\n\n";
    PrintUsage(argv[0]);
    return 1;
  }

  std::cout << "GHand EtherCAT master-slave dual-hand demo\n";
  std::cout << "Master: this process / SOEM\n";
  std::cout << "Adapter: " << adapter << '\n';
  std::cout << "Left slave: " << left_slave << '\n';
  std::cout << "Right slave: " << right_slave << "\n\n";

  auto master = ghand::GHand::Create(ghand::ProductType::GHand5,
                                     ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create EtherCAT master\n";
    return 1;
  }

  std::cout << "Connecting EtherCAT bus...\n";
  if (!master->Connect(adapter, static_cast<uint8_t>(left_slave))) {
    std::cerr << "Failed to connect EtherCAT bus on " << adapter << ": "
              << master->GetLastErrorMessage() << '\n';
    return 1;
  }

  std::cout << "Connected. Device info for slave " << left_slave
            << ": serial="
            << master->GetDeviceInfo().serial_number << '\n';

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

  master->StopSlave(right_slave);
  master->StopSlave(left_slave);
  master->Disconnect();

  std::cout << "Demo completed.\n";
  return 0;
}
