#include <iostream>

#include "ghand/ghand.h"

int main() {
  auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                      ghand::CommType::CANFD);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << '\n';
    return -1;
  }

  // Try to auto-connect to the dexterous hand via CANFD
  std::cout << "Connecting to dexterous hand via CANFD..." << '\n';
  bool success = hand->AutoConnect();

  if (success) {
    std::cout << "Successfully connected to the dexterous hand!" << '\n';

    // Get hand type
    ghand::HandType type = hand->GetHandType();
    std::cout << "Hand type: " << ghand::ToString(type) << '\n';

    // Get device info
    ghand::DeviceInfo device_info = hand->GetDeviceInfo();
    std::cout << "Device name: " << device_info.device_name << '\n';
    std::cout << "Hardware version: " << device_info.hardware_version << '\n';
    std::cout << "Software version: " << device_info.software_version << '\n';
    std::cout << "Motor driver version: " << device_info.motor_driver_version << '\n';
    std::cout << "Position sensor version: " << device_info.position_sensor_version << '\n';
    std::cout << "Tactile sensor version: " << device_info.tactile_sensor_version << '\n';
    std::cout << "Backup package version: " << device_info.backup_package_version << '\n';
    std::cout << "Serial number: " << device_info.serial_number << '\n';

    // Disconnect
    hand->Disconnect();
    std::cout << "Connection closed." << '\n';
  } else {
    std::cout << "Failed to connect to the dexterous hand!" << '\n';
  }

  return 0;
}
