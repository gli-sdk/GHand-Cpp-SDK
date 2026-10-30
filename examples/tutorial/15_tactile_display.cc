// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only
#include <iostream>
#include <QApplication>
#include "ghand/ghand.h"
#include "tactile_display/tactile_display.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  auto master = ghand::Master::Create(ghand::ProductType::GHand5, ghand::CommType::ETHERCAT);
  if (!master) return 1;
  const auto connected = master->Connect("auto");
  if (!connected || connected.value.empty()) {
    std::cerr << "Connection failed: " << connected.message << '\n'; return 1;
  }
  auto* hand = connected.value.front();
  int result;
  {
    TactileDisplay display(hand);
    display.show();
    result = app.exec();
  }  // Destroy display and join its worker before releasing the hand.
  if (!master->Disconnect()) return 1;
  return result;
}
