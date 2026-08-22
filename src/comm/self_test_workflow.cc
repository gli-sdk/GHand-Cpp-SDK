// Copyright 2025 Glitech.

#include "self_test_workflow.h"

#include <chrono>
#include <thread>

#include "logging_macros.h"

namespace ghand {
namespace internal {

namespace {

constexpr int kPollIntervalMs = 10;
constexpr int kPollTimeoutMs = 1000;

uint8_t ParseState(const std::vector<uint8_t>& bytes) {
  if (bytes.size() < 2) {
    return kSelfTestStateIdle;
  }
  return bytes[1];  // Low byte of 0x0038 (big-endian register)
}

bool PollUntilSettled(uint8_t command, const SelfTestRegisterIO& io,
                      uint8_t* out_state) {
  auto deadline = std::chrono::steady_clock::now() +
                  std::chrono::milliseconds(kPollTimeoutMs);
  while (true) {
    auto bytes = io.read_registers(kSelfTestCommandRegister, 1);
    uint8_t state = ParseState(bytes);
    if (state == kSelfTestStateSuccess || state == kSelfTestStateFailed) {
      *out_state = state;
      return true;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      GHAND_LOG_ERROR("Self-test 0x" << std::hex << static_cast<int>(command)
                                     << " timed out (state=" << std::dec
                                     << static_cast<int>(state) << ")");
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
  }
}

// Runs one A0~A8 transaction. Returns false on any transport/protocol error.
bool ReadDiagnosticErrorCodes(uint8_t command, const SelfTestRegisterIO& io,
                              std::array<uint8_t, 13>* out_codes) {
  out_codes->fill(0);

  uint16_t cmd_reg = static_cast<uint16_t>(command) << 8;  // high byte = command
  if (!io.write_register(kSelfTestCommandRegister, cmd_reg)) {
    GHAND_LOG_ERROR("Self-test 0x" << std::hex << static_cast<int>(command)
                                   << " write failed");
    return false;
  }

  uint8_t state = kSelfTestStateIdle;
  // State 只用于判断事务是否结束(2 或 3 都算结束);
  // Result 不判断,只读取 error_code 数据。
  if (!PollUntilSettled(command, io, &state)) {
    return false;
  }

  auto payload = io.read_registers(kSelfTestDataRegister,
                                   kSelfTestDataRegisterCount);
  if (static_cast<int>(payload.size()) < kSelfTestDataRegisterCount * 2) {
    GHAND_LOG_ERROR("Self-test 0x" << std::hex << static_cast<int>(command)
                                   << " data read failed");
    return false;
  }

  for (int i = 0; i < 13; ++i) {
    (*out_codes)[i] = payload[i];
  }
  return true;
}

JointId MotorIndexToJoint(const ProductConfig& config, uint8_t motor_index) {
  int slot = 0;
  for (JointId joint_id : config.valid_joints) {
    if (config.joint_limits.find(joint_id) == config.joint_limits.end()) {
      continue;
    }
    if (slot == motor_index) {
      return joint_id;
    }
    ++slot;
  }
  return JointId::NUM_JOINTS;
}

std::vector<MotorDiagnosticError> CollectMotorErrors(
    const ProductConfig& config, const std::array<uint8_t, 13>& codes) {
  std::vector<MotorDiagnosticError> errors;
  for (uint8_t i = 0; i < codes.size(); ++i) {
    if (codes[i] == 0) {
      continue;
    }
    errors.push_back({i, MotorIndexToJoint(config, i), codes[i]});
  }
  return errors;
}

}  // namespace

SelfTestErrorInfo BuildSelfTestErrorInfo(const ProductConfig& config,
                                        const SelfTestRegisterIO& io) {
  SelfTestErrorInfo info;

  std::array<uint8_t, 13> codes{};
  if (!ReadDiagnosticErrorCodes(0xA0, io, &codes)) {
    return info;
  }
  info.summary = codes[0];
  if (info.summary == 0) {
    return info;
  }

  if (HasSelfTestError(info.summary, SelfTestError::Version)) {
    if (ReadDiagnosticErrorCodes(0xA1, io, &codes)) {
      info.version = codes[0];
      GHAND_LOG_INFO("Self-test error: version mismatch (0x"
                     << std::hex << static_cast<int>(info.version) << ")");
    }
  }
  if (HasSelfTestError(info.summary, SelfTestError::PositionSensor)) {
    if (ReadDiagnosticErrorCodes(0xA2, io, &codes)) {
      info.position_sensor = CollectMotorErrors(config, codes);
    }
  }
  if (HasSelfTestError(info.summary, SelfTestError::TactileSensor)) {
    if (ReadDiagnosticErrorCodes(0xA3, io, &codes)) {
      info.tactile_sensor = CollectMotorErrors(config, codes);
    }
  }
  if (HasSelfTestError(info.summary, SelfTestError::TemperatureSensor)) {
    if (ReadDiagnosticErrorCodes(0xA4, io, &codes)) {
      info.temperature = codes[0];
    }
  }
  if (HasSelfTestError(info.summary, SelfTestError::Fan)) {
    if (ReadDiagnosticErrorCodes(0xA5, io, &codes)) {
      info.fan = codes[0];
    }
  }
  if (HasSelfTestError(info.summary, SelfTestError::Zeroing)) {
    if (ReadDiagnosticErrorCodes(0xA6, io, &codes)) {
      info.zeroing = CollectMotorErrors(config, codes);
    }
  }
  if (HasSelfTestError(info.summary, SelfTestError::Motor)) {
    if (ReadDiagnosticErrorCodes(0xA8, io, &codes)) {
      info.motor = CollectMotorErrors(config, codes);
    }
  }
  return info;
}

}  // namespace internal
}  // namespace ghand
