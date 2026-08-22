#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "ghand/ghand.h"

namespace {

// ===== Formatting helpers =====

std::string SummaryName(uint8_t summary) {
  if (summary == 0) {
    return "NONE";
  }
  std::string out;
  auto append = [&out, summary](ghand::SelfTestError flag, const char* name) {
    if (ghand::HasSelfTestError(summary, flag)) {
      if (!out.empty()) {
        out += "|";
      }
      out += name;
    }
  };
  append(ghand::SelfTestError::Version, "VERSION");
  append(ghand::SelfTestError::PositionSensor, "POSITION_SENSOR");
  append(ghand::SelfTestError::TactileSensor, "TACTILE_SENSOR");
  append(ghand::SelfTestError::TemperatureSensor, "TEMPERATURE_SENSOR");
  append(ghand::SelfTestError::Fan, "FAN");
  append(ghand::SelfTestError::Zeroing, "ZEROING");
  append(ghand::SelfTestError::Motor, "MOTOR");
  return out;
}

const char* MotorErrorName(uint8_t code) {
  switch (static_cast<ghand::MotorCheckError>(code)) {
    case ghand::MotorCheckError::None:
      return "NONE";
    case ghand::MotorCheckError::HardwareOverCurrent:
      return "HARDWARE_OVER_CURRENT";
    case ghand::MotorCheckError::SoftwareOverCurrent:
      return "SOFTWARE_OVER_CURRENT";
    case ghand::MotorCheckError::BusOverCurrent:
      return "BUS_OVER_CURRENT";
    case ghand::MotorCheckError::HardwareOverVoltage:
      return "HARDWARE_OVER_VOLTAGE";
    case ghand::MotorCheckError::HardwareUnderVoltage:
      return "HARDWARE_UNDER_VOLTAGE";
    case ghand::MotorCheckError::PhaseLoss:
      return "PHASE_LOSS";
    case ghand::MotorCheckError::Stall:
      return "STALL";
    case ghand::MotorCheckError::HardwareOverTemperature:
      return "HARDWARE_OVER_TEMPERATURE";
    case ghand::MotorCheckError::CommunicationLost:
      return "COMMUNICATION_LOST";
    case ghand::MotorCheckError::SoftwareOverVoltage:
      return "SOFTWARE_OVER_VOLTAGE";
    case ghand::MotorCheckError::SoftwareUnderVoltage:
      return "SOFTWARE_UNDER_VOLTAGE";
    case ghand::MotorCheckError::SoftwareOverTemperature:
      return "SOFTWARE_OVER_TEMPERATURE";
    case ghand::MotorCheckError::PositionSensorError:
      return "POSITION_SENSOR_ERROR";
    default:
      return "UNKNOWN";
  }
}

const char* ZeroingErrorName(uint8_t code) {
  switch (static_cast<ghand::ZeroingCheckError>(code)) {
    case ghand::ZeroingCheckError::None:
      return "NONE";
    case ghand::ZeroingCheckError::MotorAbnormal:
      return "MOTOR_ABNORMAL";
    case ghand::ZeroingCheckError::FullStrokeCheckFailed:
      return "FULL_STROKE_CHECK_FAILED";
    case ghand::ZeroingCheckError::ZeroingTimeout:
      return "ZEROING_TIMEOUT";
    default:
      return "UNKNOWN";
  }
}

std::string TactileErrorName(uint8_t code) {
  if (code == 0) {
    return "NONE";
  }
  std::string out;
  auto append = [&out, code](ghand::TactileCheckError flag, const char* name) {
    if ((code & static_cast<uint8_t>(flag)) != 0) {
      if (!out.empty()) {
        out += "|";
      }
      out += name;
    }
  };
  append(ghand::TactileCheckError::ThumbTipDisconnected,
         "THUMB_TIP_DISCONNECTED");
  append(ghand::TactileCheckError::FfTipDisconnected, "FF_TIP_DISCONNECTED");
  append(ghand::TactileCheckError::MfTipDisconnected, "MF_TIP_DISCONNECTED");
  append(ghand::TactileCheckError::RfTipDisconnected, "RF_TIP_DISCONNECTED");
  append(ghand::TactileCheckError::LfTipDisconnected, "LF_TIP_DISCONNECTED");
  append(ghand::TactileCheckError::CommunicationFailed, "COMMUNICATION_FAILED");
  if (out.empty()) {
    return "UNKNOWN";
  }
  return out;
}

std::string MotorErrors(
    const std::vector<ghand::MotorDiagnosticError>& errors,
    std::function<std::string(uint8_t)> describe = nullptr) {
  if (errors.empty()) {
    return "none";
  }
  std::string out;
  char buf[160];
  for (const auto& e : errors) {
    if (!out.empty()) {
      out += "; ";
    }
    std::string desc = describe ? describe(e.error_code) : "";
    std::snprintf(buf, sizeof(buf), "motor=%d joint=%s code=0x%02X (%s)",
                  e.motor_index, ghand::ToString(e.joint_id), e.error_code,
                  desc.c_str());
    out += buf;
  }
  return out;
}

std::string SingleCode(uint8_t summary, ghand::SelfTestError flag,
                       uint8_t code, const char* description) {
  if (!ghand::HasSelfTestError(summary, flag)) {
    return "none";
  }
  if (code == 0) {
    return "0x00";
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "0x%02X (%s)", code, description);
  return buf;
}

}  // namespace

int main() {
  auto hand = ghand::DexHand::Create(ghand::ProductType::G5,
                                     ghand::CommType::ETHERCAT);
  if (!hand) {
    std::cerr << "Failed to create DexHand" << '\n';
    return -1;
  }

  std::cout << "Connecting to dexterous hand..." << '\n';
  if (!hand->AutoConnect()) {
    std::cout << "Failed to connect to the dexterous hand!" << '\n';
    return 1;
  }
  std::cout << "Connected." << '\n';

  ghand::HandState hand_info = hand->GetHandData();
  if (hand_info.error != ghand::ErrorCode::SELF_CHECK_ERROR) {
    std::cout << "No self-test error (device error="
              << ghand::ToString(hand_info.error) << ")" << '\n';
    hand->Disconnect();
    return 0;
  }

  ghand::SelfTestErrorInfo info = hand->GetSelfTestErrorInfo();

  std::cout << "Self-test failed! Error details:" << '\n';
  std::printf("  %-16s 0x%02X  (%s)\n", "summary", info.summary,
              SummaryName(info.summary).c_str());
  std::printf("  %-16s 0x%02X\n", "version", info.version);
  std::printf("  %-16s %s\n", "position_sensor",
              MotorErrors(info.position_sensor,
                          [](uint8_t code) {
                            return code == 0 ? "" : "position sensor abnormal";
                          })
                  .c_str());
  std::printf("  %-16s %s\n", "tactile_sensor",
              MotorErrors(info.tactile_sensor, TactileErrorName).c_str());
  std::printf("  %-16s %s\n", "temperature",
              SingleCode(info.summary,
                         ghand::SelfTestError::TemperatureSensor,
                         info.temperature, "temperature sensor abnormal")
                  .c_str());
  std::printf("  %-16s %s\n", "fan",
              SingleCode(info.summary, ghand::SelfTestError::Fan, info.fan,
                         "fan abnormal")
                  .c_str());
  std::printf("  %-16s %s\n", "zeroing",
              MotorErrors(info.zeroing, ZeroingErrorName).c_str());
  std::printf("  %-16s %s\n", "motor",
              MotorErrors(info.motor, MotorErrorName).c_str());

  hand->Disconnect();
  return 0;
}
