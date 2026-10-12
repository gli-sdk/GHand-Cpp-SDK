#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <initializer_list>
#include <string>
#include <vector>
#include <utility>

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
  append(ghand::SelfTestError::Motor, "MOTOR");
  append(ghand::SelfTestError::Sensor, "SENSOR");
  append(ghand::SelfTestError::Joint, "JOINT");
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

std::string FlagErrorName(
    uint8_t code,
    std::initializer_list<std::pair<uint8_t, const char*>> flags) {
  std::string out;
  for (const auto& flag : flags) {
    if ((code & flag.first) != 0) {
      if (!out.empty()) {
        out += "|";
      }
      out += flag.second;
    }
  }
  return out.empty() ? "UNKNOWN" : out;
}

std::string SensorErrorName(uint8_t code) {
  return FlagErrorName(code, {
      {static_cast<uint8_t>(ghand::SensorError::SelfCheckFailed), "SELF_CHECK_FAILED"},
      {static_cast<uint8_t>(ghand::SensorError::MappingFailed), "MAPPING_FAILED"},
      {static_cast<uint8_t>(ghand::SensorError::TravelDeviation), "TRAVEL_DEVIATION"}});
}

std::string JointErrorName(uint8_t code) {
  return FlagErrorName(code, {
      {static_cast<uint8_t>(ghand::JointError::Stall), "STALL"},
      {static_cast<uint8_t>(ghand::JointError::Timeout), "TIMEOUT"}});
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

}  // namespace

int main() {
  auto hand = ghand::GHand::Create(ghand::ProductType::GHand5,
                                   ghand::CommType::ETHERCAT);
  if (!hand) {
    std::cerr << "Failed to create GHand" << '\n';
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
  if (ghand::HasSelfTestError(info.summary, ghand::SelfTestError::Sensor)) {
    std::printf("  %-16s %s\n", "sensor",
                MotorErrors(info.sensor, SensorErrorName).c_str());
  }
  if (ghand::HasSelfTestError(info.summary, ghand::SelfTestError::Joint)) {
    std::printf("  %-16s %s\n", "joint",
                MotorErrors(info.joint, JointErrorName).c_str());
  }
  if (ghand::HasSelfTestError(info.summary, ghand::SelfTestError::Motor)) {
    std::printf("  %-16s %s\n", "motor",
                MotorErrors(info.motor, MotorErrorName).c_str());
  }

  hand->Disconnect();
  return 0;
}
