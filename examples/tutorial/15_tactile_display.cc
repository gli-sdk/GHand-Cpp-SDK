// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only
#include <iostream>
#include <QApplication>
#include <QTimer>
#include "ghand/ghand.h"
#include "tactile_display/tactile_display.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  // --preview opens an empty display without accessing any hardware.
  // --smoke additionally closes it after 250 ms for an offscreen GUI check.
  const bool preview = app.arguments().contains("--preview");
  const bool smoke = app.arguments().contains("--smoke");
  if (smoke && !preview) {
    std::cerr << "--smoke requires --preview\n"; return 1;
  }
  std::unique_ptr<ghand::Master> master;
  ghand::GHand* hand = nullptr;
  if (!preview) {
    master = ghand::Master::Create(ghand::ProductType::GHand5, ghand::CommType::ETHERCAT);
    if (!master) return 1;
    const auto connected = master->Connect("auto");
    if (!connected || connected.value.empty()) {
      std::cerr << "Connection failed: " << connected.message << '\n'; return 1;
    }
    hand = connected.value.front();
  }
  int result;
  {
    TactileDisplay display(hand);
    display.show();
    if (smoke) QTimer::singleShot(250, &display, &QWidget::close);
    result = app.exec();
  }  // Destroy display and join its worker before releasing the hand.
  if (master && !master->Disconnect()) return 1;
  return result;
}
