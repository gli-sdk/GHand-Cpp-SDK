#include <iostream>
#include <vector>

#include "ghand/ghand.h"

int main() {
  auto hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::ETHERCAT);
  if (!hand) {
    std::cerr << "Failed to create GHand" << '\n';
    return -1;
  }

  // Try to auto-connect to the dexterous hand
  std::cout << "Connecting to dexterous hand..." << '\n';
  std::vector<ghand::ConnectedHand> connected_hands = hand->Connect("auto");
  if (connected_hands.empty()) {
    std::cout << "Failed to connect to the dexterous hand!" << '\n';
    return 1;
  }

  std::cout << "Successfully connected to the dexterous hand!" << '\n';

  // Connect() already returned both, so no extra bus round-trip is needed.
  const ghand::ConnectedHand& connected = connected_hands.front();
  std::cout << "Hand type: " << ghand::ToString(connected.hand_type) << '\n';

  const ghand::DeviceInfo& device_info = connected.device_info;

  std::cout << "Device name: " << device_info.device_name << '\n';
  std::cout << "Hardware version: " << device_info.hardware_version << '\n';
  std::cout << "Software version: " << device_info.software_version << '\n';
  std::cout << "Motor driver version: " << device_info.motor_driver_version
            << '\n';
  std::cout << "Position sensor version: "
            << device_info.position_sensor_version << '\n';
  std::cout << "Tactile sensor version: "
            << device_info.tactile_sensor_version << '\n';
  std::cout << "Thumb tactile sensor version: "
            << device_info.thumb_tactile_sensor_version << '\n';
  std::cout << "Finger tactile sensor version: "
            << device_info.finger_tactile_sensor_version << '\n';
  std::cout << "Firmware package version: "
            << device_info.firmware_package_version << '\n';
  std::cout << "Serial number: " << device_info.serial_number << '\n';

  // Disconnect
  hand->Disconnect();
  std::cout << "Connection closed." << '\n';

  return 0;
}
