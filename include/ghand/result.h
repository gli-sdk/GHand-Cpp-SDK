// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#ifndef INCLUDE_GHAND_RESULT_H_
#define INCLUDE_GHAND_RESULT_H_

#include <string>
#include <utility>

#include "ghand/types.h"

namespace ghand {

/**
 * @brief Outcome of a fallible SDK call.
 *
 * The outcome travels with the call that produced it: nothing is stored on the
 * session or hand, so two calls (or two threads) cannot overwrite each other's
 * error.
 */
struct Result {
  SdkError code = SdkError::OK;
  std::string message;

  Result() = default;
  Result(SdkError error_code, const std::string& error_message)
      : code(error_code), message(error_message) {}

  operator bool() const { return code == SdkError::OK; }
};

/**
 * @brief A Result that also carries the value produced on success.
 *
 * @warning `value` is only meaningful when `code == SdkError::OK`; on failure
 * it holds a default-constructed value.
 */
template <class T>
struct ResultOf : Result {
  T value{};

  ResultOf() = default;
  ResultOf(T v) : value(std::move(v)) {}  // NOLINT: implicit by design
  ResultOf(SdkError error_code, const std::string& error_message)
      : Result(error_code, error_message) {}
};

}  // namespace ghand

#endif  // INCLUDE_GHAND_RESULT_H_
