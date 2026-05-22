#include <iostream>

#include "ghand/ghand.h"

int main() {
  auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                      ghand::CommType::ETHERCAT);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << '\n';
    return -1;
  }

  // Try to auto-connect to the dexterous hand via EtherCAT
  std::cout << "Connecting to dexterous hand via EtherCAT..." << '\n';
  bool success = hand->AutoConnect();

  if (success) {
    std::cout << "Successfully connected to the dexterous hand!" << '\n';

    // Get hand type
    ghand::HandType type = hand->GetHandType();
    std::cout << "Hand type: " << ghand::ToString(type) << '\n';

    // Get firmware version
    ghand::DeviceInfo device_info = hand->GetDeviceInfo();
    std::string version = device_info.software_version;
    if (!version.empty()) {
      std::cout << "Firmware version: " << version << '\n';
    }

    // Get motor driver version
    std::string motor_version = device_info.motor_driver_version;
    if (!motor_version.empty()) {
      std::cout << "Motor driver version: " << motor_version << '\n';
    }

    // Disconnect
    hand->Disconnect();
    std::cout << "Connection closed." << '\n';
  } else {
    std::cout << "Failed to connect to the dexterous hand!" << '\n';
  }

  return 0;
}
