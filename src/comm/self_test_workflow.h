// Copyright 2025 Glitech.

#ifndef SRC_COMM_SELF_TEST_WORKFLOW_H_
#define SRC_COMM_SELF_TEST_WORKFLOW_H_

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "ghand/types.h"
#include "product_config.h"

namespace ghand {
namespace internal {

// Register layout shared by RS485 and CANFD backends (see
// GHand_SDK_SelfTest_Error_Interface_Design §26).
constexpr uint16_t kSelfTestCommandRegister = 0x0038;
constexpr uint16_t kSelfTestDataRegister = 0x0039;
constexpr int kSelfTestDataRegisterCount = 7;  // 7 * 16 bits = 14 bytes

constexpr uint8_t kSelfTestStateIdle = 0;
constexpr uint8_t kSelfTestStateProcessing = 1;
constexpr uint8_t kSelfTestStateSuccess = 2;
constexpr uint8_t kSelfTestStateFailed = 3;

/**
 * @brief Backend primitives required by the shared self-test workflow.
 *
 * ``read_registers`` reads ``count`` holding registers and returns the raw
 * big-endian bytes (``count * 2`` bytes). Return an empty vector on failure.
 *
 * ``write_register`` writes a single holding register. Return ``false`` on
 * failure.
 */
struct SelfTestRegisterIO {
  std::function<std::vector<uint8_t>(uint16_t address, int count)> read_registers;
  std::function<bool(uint16_t address, uint16_t value)> write_register;
};

/**
 * @brief Execute the A0 summary + on-demand A2/A6/A8 flow.
 *
 * Only categories flagged by the A0 summary are queried in detail. Throws
 * ``std::runtime_error`` on transport failure or timeout. A failed query
 * must not be reported as a device with no self-test error.
 */
SelfTestErrorInfo BuildSelfTestErrorInfo(const ProductConfig& config,
                                        const SelfTestRegisterIO& io);

}  // namespace internal
}  // namespace ghand

#endif  // SRC_COMM_SELF_TEST_WORKFLOW_H_
