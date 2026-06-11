#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

#include "ghand/ghand.h"

#ifdef _WIN32
#include <windows.h>
#endif

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

int main() {
  // Enable ANSI escape sequences
  EnableAnsiColors();

  auto hand =
      ghand::DexHand::Create(ghand::ProductType::G5, ghand::CommType::CANFD);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << '\n';
    return -1;
  }

  // ANSI escape sequences for fixed-position display
  // \033[H: move to top-left corner of screen
  // \033[J: clear from cursor to end of screen
  // \033[row;colH: move to specified row and column
  const char* CLEAR_SCREEN = "\033[H\033[J";
  const char* MOVE_CURSOR = "\033[H";  // move to top-left corner

  bool first_print = true;

  // Register tactile data callback (one-time)
  hand->SetTactileDataCallback([&first_print, CLEAR_SCREEN,
                                MOVE_CURSOR](const ghand::TactileData& data) {
    if (first_print) {
      // First print, display title
      std::cout << CLEAR_SCREEN;
      std::cout << "+==================================================+"
                << '\n';
      std::cout << "|       Tactile Data - Real-time Display          |"
                << '\n';
      std::cout << "+--------------------------------------------------+"
                << '\n';
      first_print = false;
    } else {
      // Subsequent updates, move cursor to data area
      std::cout << MOVE_CURSOR;
      std::cout << "\033[4H";  // move to start of line 4
    }

    // Iterate over tactile data by region (order consistent with configuration)
    for (const auto& region : data.regions) {
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

    std::cout << "+==================================================+"
              << '\n';
    std::cout << std::flush;  // ensure immediate output
  });

  // Try to connect to the dexterous hand via CANFD
  std::cout << "Connecting to dexterous hand via CANFD..." << '\n';
  bool success = hand->AutoConnect();

  if (success) {
    std::cout << "Successfully connected to the dexterous hand!" << '\n';

    // Open tactile
    hand->OpenTactile();

    // Data is automatically pushed, no polling needed
    std::this_thread::sleep_for(std::chrono::seconds(30));

    hand->CloseTactile();
    hand->Disconnect();
    std::cout << "Connection closed." << '\n';
  } else {
    std::cout << "Failed to connect to the dexterous hand!" << '\n';
  }

  return 0;
}
