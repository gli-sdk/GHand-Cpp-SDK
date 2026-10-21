#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <thread>
#include <vector>

#include "ghand/ghand.h"

void PrintHeader() {
  std::cout << "========================================\n";
  std::cout << "  GHand Dexterous Hand SDK - Multi-Hand Control Demo\n";
  std::cout << "========================================\n";
}

bool SearchAdapters(std::map<std::string, std::string>* adapters) {
  auto temp_master = ghand::Master::Create(ghand::ProductType::GHand5,
                                           ghand::CommType::ETHERCAT);
  if (!temp_master) {
    std::cerr << "Failed to create GHand" << '\n';
    return false;
  }

  *adapters = temp_master->SearchAdapters();
  if (adapters->empty()) {
    std::cerr << "No available adapters found\n";
    return false;
  }

  std::cout << "\nFound " << adapters->size()
            << " available adapters:\n";
  for (const auto& adapter : *adapters) {
    std::cout << "  - " << adapter.first << ": " << adapter.second << '\n';
  }
  return true;
}

// One bus per adapter, and therefore one session per adapter: the masters own
// the hands, the raw pointers here are just views into them.
void ConnectHands(
    const std::map<std::string, std::string>& adapters,
    std::vector<std::unique_ptr<ghand::Master>>* masters,
    std::vector<ghand::GHand*>* hands) {
  std::set<std::string> connected_adapter_bases;
  std::cout << "\nAttempting to connect devices...\n";

  for (const auto& adapter : adapters) {
    const std::string& name = adapter.first;
    std::string adapter_base = name.substr(0, name.find(':'));
    if (connected_adapter_bases.count(adapter_base) != 0) {
      std::cout << "Skipping alternate channel: " << name << '\n';
      continue;
    }

    auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                        ghand::CommType::ETHERCAT);
    const auto connected = master->Connect(name);
    if (connected) {
      hands->insert(hands->end(), connected.value.begin(), connected.value.end());
      masters->push_back(std::move(master));
      connected_adapter_bases.insert(adapter_base);
      std::cout << "Connected device: " << name << '\n';
    } else {
      std::cout << "Connection failed: " << name << " (" << connected.message
                << ")\n";
    }
  }
}

void DisplayDeviceInfo(const std::vector<ghand::GHand*>& hands) {
  std::cout << "\nDevice Information:\n";
  for (size_t i = 0; i < hands.size(); ++i) {
    const ghand::DeviceInfo info = hands[i]->device_info();
    std::cout << "  Hand " << i << ":\n";
    std::cout << "    Device Name: " << info.device_name << '\n';
    std::cout << "    Hardware Version: " << info.hardware_version << '\n';
    std::cout << "    Software Version: " << info.software_version << '\n';
    std::cout << "    Serial Number: " << info.serial_number << '\n';
  }
}

void RunIndividualControl(const std::vector<ghand::GHand*>& hands) {
  std::cout << "\n========== Demo 1: Individual Control ==========\n";
  std::vector<ghand::JointCommand> test_joints = {
      {ghand::JointId::FF_PIP, 45.0f, 100, 100},
      {ghand::JointId::FF_MCP, 30.0f, 100, 100},
  };

  for (size_t i = 0; i < hands.size(); ++i) {
    std::cout << "\nControlling hand " << i << "...\n";
    const auto result = hands[i]->MoveJoints(test_joints);
    if (result) {
      std::cout << "Command sent successfully\n";
    } else {
      std::cout << "Command send failed: " << result.message << '\n';
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  std::cout << "\nHolding position for 2 seconds...\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));
}

void RunSimultaneousControl(const std::vector<ghand::GHand*>& hands) {
  std::cout << "\n========== Demo 2: Simultaneous Control ==========\n";
  std::vector<ghand::JointCommand> reset_joints = {
      {ghand::JointId::THUMB_MCP, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_FE, 0.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_AA, 20.0f, 100, 100},
      {ghand::JointId::THUMB_TMC_PS, 0.0f, 100, 100},
      {ghand::JointId::FF_PIP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP, 0.0f, 100, 100},
      {ghand::JointId::FF_MCP_AA, 0.0f, 100, 100},
      {ghand::JointId::MF_PIP, 0.0f, 100, 100},
      {ghand::JointId::MF_MCP, 0.0f, 100, 100},
      {ghand::JointId::RF_PIP, 0.0f, 100, 100},
      {ghand::JointId::RF_MCP, 0.0f, 100, 100},
      {ghand::JointId::LF_PIP, 0.0f, 100, 100},
      {ghand::JointId::LF_MCP, 0.0f, 100, 100},
  };

  std::cout << "\nResetting all hands simultaneously...\n";
  for (ghand::GHand* hand : hands) {
    hand->MoveJoints(reset_joints);
  }
  std::cout << "Commands sent to all hands\n";
  std::cout << "\nWaiting for movement completion...\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));
}

void DisconnectHands(std::vector<std::unique_ptr<ghand::Master>>* masters) {
  std::cout << "\n========== Cleanup ==========\n";
  std::cout << "\nDisconnecting all connections...\n";
  for (size_t i = 0; i < masters->size(); ++i) {
    (*masters)[i]->Disconnect();
    std::cout << "Closed hand " << i << '\n';
  }
}

int main() {
  PrintHeader();

  std::map<std::string, std::string> adapters;
  if (!SearchAdapters(&adapters)) return 1;

  std::vector<std::unique_ptr<ghand::Master>> masters;
  std::vector<ghand::GHand*> hands;
  ConnectHands(adapters, &masters, &hands);
  if (hands.empty()) {
    std::cerr << "\nFailed to connect any device\n";
    return 1;
  }

  std::cout << "\nSuccessfully connected " << hands.size() << " device(s)\n";
  DisplayDeviceInfo(hands);
  RunIndividualControl(hands);
  RunSimultaneousControl(hands);
  DisconnectHands(&masters);

  std::cout << "\nDemo completed. Thank you for using!\n";
  return 0;
}
