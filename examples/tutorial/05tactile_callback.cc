#ifdef _WIN32
#include <windows.h>
#endif

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include "ghand/ghand.h"

const char kClearScreen[] = "\033[H\033[J";
const char kMoveCursor[] = "\033[H";

// Enable ANSI escape sequence support (Windows)
void EnableAnsiColors() {
#ifdef _WIN32
  // Set console code page to UTF-8
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut != INVALID_HANDLE_VALUE) {
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
      dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode(hOut, dwMode);
    }
  }
#endif
}

void PrintTactileHeader() {
  std::cout << kClearScreen;
  std::cout << "+==================================================+" << '\n';
  std::cout << "|       Tactile Data - Real-time Display          |" << '\n';
  std::cout << "+--------------------------------------------------+" << '\n';
}

void PrintTactileRegion(const ghand::RegionTactile& region) {
  std::cout << "| " << std::setw(6) << region.region_name << ": "
            << "state=" << std::setw(6) << (region.state ? "OK" : "FAIL")
            << ", "
            << "x=" << std::setw(6) << std::fixed << std::setprecision(1)
            << region.resultant_force.x << ", y=" << std::setw(6)
            << std::fixed << std::setprecision(1)
            << region.resultant_force.y << ", z=" << std::setw(6)
            << std::fixed << std::setprecision(1)
            << region.resultant_force.z << " N |" << '\n';
}

void PrintTactileStatus(const ghand::TactileData& data) {
  std::ostringstream status;
  status << "sensor_state=0x" << std::hex << std::uppercase
         << std::setw(2) << std::setfill('0')
         << static_cast<int>(data.sensor_state)
         << ", sensor_error=0x" << std::setw(2)
         << static_cast<int>(data.sensor_error);
  std::cout << "| " << std::left << std::setw(48) << std::setfill(' ')
            << status.str() << std::right << " |" << '\n';
}

void PrintTactileData(const ghand::TactileData& data, bool* first_print) {
  if (*first_print) {
    PrintTactileHeader();
    *first_print = false;
  } else {
    std::cout << kMoveCursor;
    std::cout << "\033[4H";
  }

  PrintTactileStatus(data);
  for (const auto& region : data.regions) {
    PrintTactileRegion(region);
  }

  std::cout << "+==================================================+" << '\n';
  std::cout << std::flush;
}

int main() {
  EnableAnsiColors();

  auto master = ghand::Master::Create(ghand::ProductType::GHand5,
                                      ghand::CommType::ETHERCAT);
  if (!master) {
    std::cerr << "Failed to create GHand" << '\n';
    return -1;
  }

  std::cout << "Connecting to dexterous hand..." << '\n';
  const auto connected = master->Connect("auto");
  if (!connected) {
    std::cerr << "Failed to connect to the dexterous hand: "
              << connected.message << '\n';
    return 1;
  }
  std::cout << "Successfully connected to the dexterous hand!" << '\n';

  ghand::GHand* hand = connected.value.front();

  const auto open_result = hand->OpenTactile();
  if (!open_result) {
    std::cerr << "Failed to open tactile sensor: " << open_result.message
              << '\n';
    master->Disconnect();
    return 1;
  }
  std::cout << "Tactile sensor opened." << '\n';

  const auto zero_result = hand->ZeroTactile();
  if (!zero_result) {
    std::cerr << "Failed to zero tactile sensor: " << zero_result.message
              << '\n';
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // The callback is registered on one hand, so it only ever receives that
  // hand's data.
  bool first_print = true;
  hand->SetTactileDataCallback([&first_print](const ghand::TactileData& data) {
    PrintTactileData(data, &first_print);
  });

  std::this_thread::sleep_for(std::chrono::seconds(30));

  hand->CloseTactile();
  master->Disconnect();
  std::cout << "Connection closed." << '\n';

  return 0;
}
