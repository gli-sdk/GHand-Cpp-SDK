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

void RegisterTactileCallback(ghand::DexHand* hand) {
  bool first_print = true;
  hand->SetTactileDataCallback(
      [first_print](const ghand::TactileData& data) mutable {
        PrintTactileData(data, &first_print);
      });
}

void RunTactileDemo(ghand::DexHand* hand) {
  std::cout << "Connecting to dexterous hand..." << '\n';
  if (!hand->AutoConnect()) {
    std::cout << "Failed to connect to the dexterous hand!" << '\n';
    return;
  }

  std::cout << "Successfully connected to the dexterous hand!" << '\n';
  if (!hand->OpenTactile()) {
    std::cerr << "Failed to open tactile sensor!" << '\n';
    hand->Disconnect();
    return;
  }
  std::cout << "Tactile sensor opened." << '\n';
  if (!hand->ZeroTactile()) {
    std::cerr << "Failed to zero tactile sensor!" << '\n';
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  RegisterTactileCallback(hand);

  std::this_thread::sleep_for(std::chrono::seconds(30));
  hand->CloseTactile();
  hand->Disconnect();
  std::cout << "Connection closed." << '\n';
}

int main() {
  EnableAnsiColors();

  auto hand =
      ghand::DexHand::Create(ghand::ProductType::G5, ghand::CommType::ETHERCAT);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << '\n';
    return -1;
  }

  RunTactileDemo(hand.get());
  return 0;
}
