// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <iostream>

#include "ghand/ghand.h"

int main() {
  auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create GHand" << '\n';
    return -1;
  }

  // Try to auto-connect to the dexterous hand
  std::cout << "Connecting to dexterous hand..." << '\n';
  const auto connected = master->Connect("auto");
  if (!connected) {
    std::cerr << "Failed to connect to the dexterous hand: "
              << connected.message << '\n';
    return 1;
  }

  std::cout << "Successfully connected to the dexterous hand!" << '\n';

  // The hand carries what Connect() found, so no extra bus round-trip is
  // needed. The pointer stays valid until the next Connect()/Disconnect().
  ghand::GHand* hand = connected.value.front();
  std::cout << "Hand type: " << ghand::ToString(hand->hand_type()) << '\n';

  // device_info() returns the snapshot taken during Connect(). Call
  // QueryDeviceInfo() if you need a fresh read from the device.
  const ghand::DeviceInfo device_info = hand->device_info();

  std::cout << "Device name: " << device_info.device_name << '\n';
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
  const auto disconnected = master->Disconnect();
  if (!disconnected) {
    std::cerr << "Failed to disconnect: " << disconnected.message << '\n';
    return 1;
  }
  std::cout << "Connection closed." << '\n';

  return 0;
}
