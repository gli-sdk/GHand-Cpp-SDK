// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only
#ifndef GHAND_EXAMPLES_EXAMPLE_OPTIONS_H_
#define GHAND_EXAMPLES_EXAMPLE_OPTIONS_H_

#include <iostream>
#include <string>
#include "ghand/types.h"

namespace example {
struct Options {
  ghand::ProductType product;
  ghand::CommType comm;
  std::string device;
};

enum class ParseResult { Run, Help, Error };

inline bool ParseComm(const std::string& value, ghand::CommType& comm) {
  if (value == "ethercat" || value == "ethcat")
    comm = ghand::CommType::ETHERCAT;
  else if (value == "canfd") comm = ghand::CommType::CANFD;
  else if (value == "rs485") comm = ghand::CommType::RS485;
  else return false;
  return true;
}

inline void PrintUsage(const char* program, const Options& defaults,
                       bool canfd_only = false,
                       bool legacy = false) {
  std::cout << "Usage: " << program
            << " [--product ghand5|ghandlite1|auto]"
            << " [--comm ethercat|canfd|rs485] [--device DEVICE]\n"
            << "Defaults: product="
            << (defaults.product == ghand::ProductType::GHand5 ? "ghand5" :
                defaults.product == ghand::ProductType::GHandLite1 ? "ghandlite1" : "auto")
            << ", comm="
            << (defaults.comm == ghand::CommType::ETHERCAT ? "ethercat" :
                defaults.comm == ghand::CommType::CANFD ? "canfd" : "rs485")
            << ", device=" << defaults.device << '\n'
            << "  -h, --help  Show this help without connecting.\n";
  if (canfd_only)
    std::cout << "This baud-rate example supports CAN-FD only.\n";
  if (legacy)
    std::cout << "Also accepts: [ethcat|canfd|rs485] [device]\n";
}

inline ParseResult ParseOptions(int argc, char** argv, Options& options,
                                bool canfd_only = false, bool legacy = false) {
  const auto defaults = options;
  bool positional_comm = false;
  bool device_set = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0], defaults, canfd_only, legacy);
      return ParseResult::Help;
    }
    if (arg == "--product" || arg == "--comm" || arg == "--device") {
      if (i + 1 >= argc || std::string(argv[i + 1]).rfind("-", 0) == 0) {
        std::cerr << "Missing value for " << arg << '\n';
        return ParseResult::Error;
      }
      const std::string value = argv[++i];
      if (arg == "--product") {
        if (value == "ghand5") options.product = ghand::ProductType::GHand5;
        else if (value == "ghandlite1") options.product = ghand::ProductType::GHandLite1;
        else if (value == "auto") options.product = ghand::ProductType::AUTO;
        else {
          std::cerr << "Unknown product: " << value << '\n';
          return ParseResult::Error;
        }
      } else if (arg == "--comm") {
        if (!ParseComm(value, options.comm)) {
          std::cerr << "Unknown communication type: " << value << '\n';
          return ParseResult::Error;
        }
        positional_comm = true;
      } else {
        options.device = value;
        device_set = true;
      }
    } else if (legacy && !arg.empty() && arg.front() != '-') {
      if (!positional_comm && !device_set && ParseComm(arg, options.comm)) {
        positional_comm = true;
      } else if (!device_set) {
        options.device = arg;
        device_set = true;
      } else {
        std::cerr << "Unexpected argument: " << arg << '\n';
        return ParseResult::Error;
      }
    } else {
      std::cerr << "Unknown argument: " << arg << " (use --help)\n";
      return ParseResult::Error;
    }
  }
  if (canfd_only && options.comm != ghand::CommType::CANFD) {
    std::cerr << "This baud-rate example supports --comm canfd only.\n";
    return ParseResult::Error;
  }
  return ParseResult::Run;
}
}  // namespace example
#endif  // GHAND_EXAMPLES_EXAMPLE_OPTIONS_H_
