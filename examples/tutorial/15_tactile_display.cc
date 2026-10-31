// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only
#include <QApplication>
#include <iostream>

#include "ghand/ghand.h"
#include "../example_options.h"
#include "tactile_display/tactile_display.h"

int main(int argc, char** argv) {
  example::Options options{
      ghand::ProductType::GHand5,
      ghand::CommType::ETHERCAT,
      "auto",
  };
  const auto parsed = example::ParseOptions(argc, argv, options);
  if (parsed != example::ParseResult::Run)
    return parsed == example::ParseResult::Help ? 0 : 1;

  QApplication app(argc, argv);
  auto master = ghand::Master::Create(options.product, options.comm);
  if (!master) return 1;
  const auto connected = master->Connect(options.device);
  if (!connected || connected.value.empty()) {
    std::cerr << "Connection failed: " << connected.message << '\n';
    return 1;
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
