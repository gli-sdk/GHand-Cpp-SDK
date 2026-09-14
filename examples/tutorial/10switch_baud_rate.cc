#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

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

void PrintLastError(const ghand::GHand& hand) {
  const auto result = hand.GetLastResult();
  std::cerr << "Last error: " << static_cast<int>(result.code) << " "
            << result.message << '\n';
}

bool RunCanfdExample() {
  const auto current_timing = ghand::CANFDBitTiming::TIMING_1M_5M;
  const auto target_timing = ghand::CANFDBitTiming::TIMING_1M_4M;

  auto hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                   ghand::CommType::CANFD);
  if (!hand) {
    std::cerr << "Failed to create GHand" << '\n';
    return false;
  }

  std::cout << "Connecting with current CANFD timing "
            << CanfdTimingName(current_timing) << "..." << '\n';
  if (!hand->Connect(kDeviceName, kSlaveId, current_timing)) {
    std::cerr << "Connection failed" << '\n';
    PrintLastError(*hand);
    return false;
  }

  std::cout << "Writing new CANFD timing " << CanfdTimingName(target_timing)
            << "..." << '\n';
  if (!hand->SetBaudrateConfig(target_timing)) {
    std::cerr << "Failed to write baud-rate configuration" << '\n';
    PrintLastError(*hand);
    hand->Disconnect();
    return false;
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));
  hand->Disconnect();

  std::cout << "Baud-rate configuration written successfully." << '\n';
  std::cout << "Power-cycle the device, then reconnect with:" << '\n';
  std::cout << "  hand->Connect(\"" << kDeviceName << "\", 0x" << std::hex
            << static_cast<int>(kSlaveId) << std::dec << ", "
            << "ghand::CANFDBitTiming::" << CanfdTimingName(target_timing)
            << ")" << '\n';
  return true;
}

}  // namespace

int main() {
  return RunCanfdExample() ? 0 : 1;
}
