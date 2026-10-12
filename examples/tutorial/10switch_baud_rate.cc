#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "ghand/ghand.h"

namespace {

constexpr const char* kDeviceName = "auto";
constexpr uint8_t kSlaveId = 0x31;

std::string CanfdTimingName(ghand::CANFDBitTiming timing) {
  switch (timing) {
    case ghand::CANFDBitTiming::TIMING_500K_1M:
      return "TIMING_500K_1M";
    case ghand::CANFDBitTiming::TIMING_500K_2M:
      return "TIMING_500K_2M";
    case ghand::CANFDBitTiming::TIMING_500K_4M:
      return "TIMING_500K_4M";
    case ghand::CANFDBitTiming::TIMING_500K_5M:
      return "TIMING_500K_5M";
    case ghand::CANFDBitTiming::TIMING_1M_4M:
      return "TIMING_1M_4M";
    case ghand::CANFDBitTiming::TIMING_1M_5M:
      return "TIMING_1M_5M";
  }
  return "UNKNOWN";
}

}  // namespace

int main() {
  const auto current_timing = ghand::CANFDBitTiming::TIMING_1M_5M;
  const auto target_timing = ghand::CANFDBitTiming::TIMING_1M_4M;

  auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::CANFD);
  if (!master) {
    std::cerr << "Failed to create GHand" << '\n';
    return 1;
  }

  std::cout << "Connecting with current CANFD timing "
            << CanfdTimingName(current_timing) << "..." << '\n';
  const auto connected = master->Connect(kDeviceName, kSlaveId, current_timing);
  if (!connected) {
    std::cerr << "Connection failed: " << connected.message << '\n';
    return 1;
  }
  ghand::GHand* hand = connected.value.front();

  std::cout << "Writing new CANFD timing " << CanfdTimingName(target_timing)
            << "..." << '\n';
  const auto write_result = hand->SetBaudrateConfig(target_timing);
  if (!write_result) {
    std::cerr << "Failed to write baud-rate configuration: "
              << write_result.message << '\n';
    master->Disconnect();
    return 1;
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));
  master->Disconnect();

  std::cout << "Baud-rate configuration written successfully." << '\n';
  std::cout << "Power-cycle the device, then reconnect with:" << '\n';
  std::cout << "  master->Connect(\"" << kDeviceName << "\", 0x" << std::hex
            << static_cast<int>(kSlaveId) << std::dec << ", "
            << "ghand::CANFDBitTiming::" << CanfdTimingName(target_timing)
            << ")" << '\n';
  return 0;
}
