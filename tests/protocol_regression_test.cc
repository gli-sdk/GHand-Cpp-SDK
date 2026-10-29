// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <modbus/modbus.h>
#include <nlohmann/json.hpp>

#include "canfd_comm.h"
#include "ghand/ghand.h"
#include "ethercat_comm.h"
#include "modbus_codec.h"
#include "product_config_loader.h"
#include "rs485_comm.h"
// Compile the SOEM implementation here so the friend fixture can seed an in-memory bus.
#define ecx_SDOread SimulatedSDORead
#define ecx_SDOwrite SimulatedSDOWrite
#include "../src/comm/ethercat_comm.cc"
#undef ecx_SDOread
#undef ecx_SDOwrite

using namespace ghand;
using namespace ghand::internal;
using nlohmann::json;

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error( \
    std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #condition); } while (false)

namespace {
struct Write {
  int address;
  std::vector<uint16_t> values;
};
struct Simulator {
  std::map<int, uint16_t> inputs;
  std::map<int, uint16_t> holding;
  std::vector<uint16_t> pid_parameters = std::vector<uint16_t>(39);
  std::vector<Write> writes;
  std::atomic<uint16_t> terminal{0x0100};
  std::atomic<bool> fail_write{false};
  std::atomic<bool> online{true};
  void Apply(int address, const std::vector<uint16_t>& values) {
    writes.push_back({address, values});
    for (size_t i = 0; i < values.size(); ++i) holding[address + static_cast<int>(i)] = values[i];
    if (address == 0x21 && terminal == 0x0100) {
      auto& parameters = pid_parameters;
      const int start = 0x315;
      for (size_t i = 0; i < parameters.size(); ++i) {
        if (values[0] == 1) parameters[i] = holding[start + static_cast<int>(i)];
        if (values[0] == 2) holding[start + static_cast<int>(i)] = parameters[i];
      }
    }
    int mailbox = address == 0 ? 0x1026 : address == 0x1F ? 0x1027 :
                  address == 0x20 ? 0x1028 : address == 3 ? 0x1029 :
                  address == 0x21 ? 0x102B : address == 0x22 ? 0x102C : 0x102A;
    inputs[mailbox] = terminal.load();
    inputs[0x1047] = 1;
  }
};
std::map<uint16_t, Simulator> serial_nodes;
std::mutex serial_mutex;
std::atomic<bool> serial_connect_fail{false};
void SeedNode(Simulator& node, uint16_t id) {
  node.inputs[0x1012] = id == 0x32 ? 2 : 1;
  std::string name = "GHand 5";
  for (size_t i = 0; i < name.size(); ++i) {
    int address = 0x100A + static_cast<int>(i / 2);
    node.inputs[address] |= static_cast<uint16_t>(static_cast<uint8_t>(name[i])) << (i % 2 == 0 ? 8 : 0);
  }
  node.inputs[0x105E] = id == 0x32 ? 30 : 20;
  node.inputs[0x1066] = id == 0x32 ? 200 : 100;
}

uint32_t CanId(int src, int dst, int ack, int fc, int start = 1, int end = 1, int seg = 0) {
  return (src << 23) | (dst << 17) | (ack << 16) | (fc << 8) |
         (start << 7) | (end << 6) | ((seg % 2) << 5) | seg;
}

class FakeCanDriver : public CANFDDriver {
 public:
  std::map<uint16_t, Simulator> nodes;
  FakeCanDriver() { SeedNode(nodes[0x31], 0x31); SeedNode(nodes[0x32], 0x32); }

  std::deque<canfd::Frame> replies;
  std::vector<uint8_t> pending;
  std::mutex mutex;
  std::atomic<bool> short_reply{false};
  std::atomic<int> keepalive_reads{0};
  bool open = false;
  std::atomic<bool> fail_open{false};
  int Open(const std::string&, uint32_t, uint32_t, uint8_t, uint8_t) override {
    if (fail_open) return -1;
    open = true;
    replies.push_back(Frame(CanId(0x31, 0x31, 0, 0x44), {}));
    return 0;
  }
  void Close() override { open = false; }
  bool IsOpen() const override { return open; }
  std::map<std::string, std::string> EnumerateAdapters() override { return {}; }
  static canfd::Frame Frame(uint32_t id, const std::vector<uint8_t>& bytes) {
    canfd::Frame frame;
    frame.id = id;
    frame.len = static_cast<uint8_t>(bytes.size());
    std::copy(bytes.begin(), bytes.end(), frame.data);
    return frame;
  }
  int Send(const canfd::Frame& frame) override {
    std::lock_guard<std::mutex> lock(mutex);
    int fc = (frame.id >> 8) & 0xFF;
    int src = (frame.id >> 23) & 0x3F, dst = (frame.id >> 17) & 0x3F;
    auto& sim = nodes[dst];
    if (!sim.online) return -1;
    if (sim.fail_write && (fc == 6 || fc == 0x10)) return -1;
    if (fc == 0x44) {
      CHECK(frame.data[4] == dst);
      replies.push_back(Frame(CanId(dst, src, 1, fc), {}));
      return 0;
    }
    if (fc == 3 || fc == 4) {
      int address = (frame.data[0] << 8) | frame.data[1];
      if (address == 0x1047) ++keepalive_reads;
      int count = (frame.data[2] << 8) | frame.data[3];
      std::vector<uint8_t> data(1, static_cast<uint8_t>(count * 2));
      for (int i = 0; i < count; ++i) {
        uint16_t value = fc == 3 ? sim.holding[address + i] : sim.inputs[address + i];
        data.push_back(static_cast<uint8_t>(value >> 8));
        data.push_back(static_cast<uint8_t>(value));
      }
      if (short_reply) replies.push_back(Frame(CanId(dst, src, 1, fc), {2, 0}));
      int segments = static_cast<int>((data.size() + 63) / 64);
      for (int i = 0; i < segments; ++i) {
        size_t end = std::min(data.size(), static_cast<size_t>((i + 1) * 64));
        replies.push_back(Frame(CanId(dst, src, 1, fc, i == 0, i == segments - 1, i),
            std::vector<uint8_t>(data.begin() + i * 64, data.begin() + end)));
      }
      return 0;
    }
    if ((frame.id >> 7) & 1) pending.clear();
    pending.insert(pending.end(), frame.data, frame.data + frame.len);
    if (!((frame.id >> 6) & 1)) return 0;
    int address = (pending[0] << 8) | pending[1];
    int count = fc == 6 ? 1 : (pending[2] << 8) | pending[3];
    size_t offset = fc == 6 ? 2 : 5;
    std::vector<uint16_t> values;
    for (int i = 0; i < count; ++i)
      values.push_back(static_cast<uint16_t>((pending[offset + i * 2] << 8) | pending[offset + i * 2 + 1]));
    sim.Apply(address, values);
    if (address == 3) {
      auto next = static_cast<uint16_t>(values[0] >> 8);
      SeedNode(nodes[next], next);
      nodes[next].inputs[0x1029] = 0x0100;
      sim.online = false;
    }
    replies.push_back(Frame(CanId(dst, src, 1, fc), std::vector<uint8_t>(pending.begin(), pending.begin() + 4)));
    return 0;
  }
  int Receive(canfd::Frame* frame, int) override {
    std::lock_guard<std::mutex> lock(mutex);
    if (replies.empty()) return -1;
    *frame = replies.front();
    replies.pop_front();
    return 0;
  }
};
FakeCanDriver* can_driver = nullptr;
std::map<uint16_t, uint8_t> sdo_commands;
uint8_t simulated_machine = 2;
}  // namespace

extern "C" int SimulatedSDORead(ecx_contextt*, uint16, uint16 index, uint8 subindex,
    boolean, int* size, void* data, int) {
  if (index == 0x2009 && subindex == 1 && *size >= 19) {
    std::memcpy(data, "1234567890123456789", 19); *size = 19; return 1;
  }
  if (index == 0x200A && subindex == 1 && *size >= 1) {
    *static_cast<uint8_t*>(data) = simulated_machine; *size = 1; return 1;
  }
  if (index == 0x2003 || index == 0x2008) {
    if (subindex == 3 || subindex == 4) {
      *static_cast<uint8_t*>(data) = subindex == 3 ? 0 : 1; *size = 1; return 1;
    }
    const int count = index == 0x2003 ? 13 : 78;
    if (subindex == 1 && *size >= count) {
      for (int i = 0; i < count; ++i) static_cast<uint8_t*>(data)[i] = static_cast<uint8_t>(i + 1);
      *size = count; return 1;
    }
  }
  return -1;
}

extern "C" int SimulatedSDOWrite(ecx_contextt*, uint16, uint16 index, uint8 subindex,
    boolean, int size, const void* data, int) {
  if ((index == 0x2003 || index == 0x2008) && subindex == 2 && size == 1) {
    sdo_commands[index] = *static_cast<const uint8_t*>(data); return 1;
  }
  return -1;
}

// Replace external I/O at link time. No serial port or CAN hardware is opened.
struct _modbus { uint16_t slave = 0x31; };
extern "C" {
modbus_t* modbus_new_rtu(const char*, int, char, int, int) { return new _modbus; }
int modbus_set_slave(modbus_t* ctx, int slave) { ctx->slave = static_cast<uint16_t>(slave); return 0; }
int modbus_set_response_timeout(modbus_t*, uint32_t, uint32_t) { return 0; }
int modbus_connect(modbus_t*) { return serial_connect_fail ? -1 : 0; }
void modbus_close(modbus_t*) {}
void modbus_free(modbus_t* ctx) { delete ctx; }
const char* modbus_strerror(int) { return "simulated error"; }
int modbus_read_registers(modbus_t* ctx, int address, int count, uint16_t* out) {
  std::lock_guard<std::mutex> lock(serial_mutex);
  if (!serial_nodes[ctx->slave].online) return -1;
  for (int i = 0; i < count; ++i) out[i] = serial_nodes[ctx->slave].holding[address + i];
  return count;
}
int modbus_read_input_registers(modbus_t* ctx, int address, int count, uint16_t* out) {
  std::lock_guard<std::mutex> lock(serial_mutex);
  auto& node = serial_nodes[ctx->slave];
  if (!node.online) return -1;
  for (int i = 0; i < count; ++i) out[i] = node.inputs[address + i];
  return count;
}
int modbus_write_register(modbus_t* ctx, int address, uint16_t value) {
  std::lock_guard<std::mutex> lock(serial_mutex);
  auto& node = serial_nodes[ctx->slave];
  if (!node.online || node.fail_write) return -1;
  node.Apply(address, {value});
  if (address == 3) {
    auto next = static_cast<uint16_t>(value >> 8);
    SeedNode(serial_nodes[next], next); serial_nodes[next].inputs[0x1029] = 0x0100;
    node.online = false;
  }
  return 1;
}
int modbus_write_registers(modbus_t* ctx, int address, int count, const uint16_t* values) {
  std::lock_guard<std::mutex> lock(serial_mutex);
  auto& node = serial_nodes[ctx->slave];
  if (!node.online || node.fail_write) return -1;
  node.Apply(address, std::vector<uint16_t>(values, values + count)); return count;
}
}

namespace ghand {
namespace internal {
std::unique_ptr<CANFDDriver> CreateCANFDDriver() {
  auto driver = std::unique_ptr<FakeCanDriver>(new FakeCanDriver);
  can_driver = driver.get();
  return std::unique_ptr<CANFDDriver>(driver.release());
}
struct ProtocolTestAccess {
  static void SeedEthercat(EtherCATComm& comm) {
    comm.soem_->ctx.slavecount = 3;
    comm.is_connected_ = true;
    for (uint16_t id : {2, 3}) {
      ConnectedHand hand; hand.slave_index = id; hand.hand_type = id == 2 ? HandType::LEFT_HAND : HandType::RIGHT_HAND;
      comm.hands_.push_back(hand);
      comm.soem_->ctx.slavelist[id].Obytes = static_cast<uint32_t>(2 + 12 * comm.config_.joint_limits.size());
    }
  }
  static void CleanupEthercat(EtherCATComm* comm) {
    comm->soem_->ctx.slavecount = 0; comm->is_connected_ = false; delete comm;
  }
  static std::vector<uint8_t> Output(EtherCATComm& comm, uint16_t id) {
    return std::vector<uint8_t>(comm.rxpdo_buffers_[id].begin(), comm.rxpdo_buffers_[id].begin() + comm.rxpdo_sizes_[id]);
  }
  static void ParseSlave(EtherCATComm& comm, const std::vector<uint8_t>& bytes, uint16_t id) {
    comm.ParseAndNotify(bytes.data(), bytes.size(), id);
  }
  static bool Read(CANFDComm& comm, int address, int count, std::vector<uint8_t>* bytes) {
    return comm.ReadRegisters(0x31, address, count, bytes, 4, 20);
  }
  static void Parse(EtherCATComm& comm, const std::vector<uint8_t>& bytes) {
    comm.ParseAndNotify(bytes.data(), bytes.size(), 2);
  }
  static std::vector<JointData> Joints(RS485Comm& comm) { return comm.GetJoints(0x31); }
};
}  // namespace internal
}  // namespace ghand

void CheckVectors(const json& vectors) {
  auto raw = vectors["hand_raw"].get<std::vector<uint16_t>>();
  auto hand = ParseHandInfo(raw.data(), raw.size());
  CHECK(std::vector<uint8_t>(hand.error_fault.begin(), hand.error_fault.end()) == vectors["hand_fault"].get<std::vector<uint8_t>>());
  CHECK(hand.temperature == vectors["hand_temperature"].get<int>());
  raw = vectors["joint_raw"].get<std::vector<uint16_t>>();
  auto joint = ParseJointData(raw.data(), JointId::THUMB_MCP);
  CHECK(joint.angle == vectors["joint_angle"].get<float>());
  CHECK(joint.velocity == vectors["joint_speed"].get<int>());
  CHECK(joint.current == vectors["joint_current"].get<int>());
  CHECK(joint.control_mode == JointControlMode::MIT && joint.loop_type == JointLoopType::SEMI_CLOSED_LOOP);
  CHECK(ParseJointData(raw.data(), JointId::THUMB_MCP).control_mode == JointControlMode::MIT);
  raw = vectors["tactile_raw"].get<std::vector<uint16_t>>();
  auto force = ParseTactileResultant(raw.data(), 0);
  CHECK(force[0] == vectors["resultant"][0].get<float>());
  CHECK(force[1] == vectors["resultant"][1].get<float>());
  CHECK(force[2] == vectors["resultant"][2].get<float>());
  auto state = ParseTactileStateError(raw[0]);
  CHECK(state.first == 1 && state.second == 9);
  auto bytes = vectors["distributed_raw"].get<std::vector<uint8_t>>();
  auto sample = ParseTactileDistributed(bytes.data(), bytes.size(), 1);
  CHECK(sample[0] == vectors["distributed"][0].get<float>());
  CHECK(sample[2] == vectors["distributed"][2].get<float>());
  const uint8_t boundary_force[] = {0x80, 0x7F, 0xFF};
  sample = ParseTactileDistributed(boundary_force, sizeof(boundary_force), 1);
  CHECK(sample[0] == -12.8f && sample[1] == 12.7f && sample[2] == 25.5f);
  for (const auto& product : vectors["products"]) {
    auto type = product["name"] == "GHand5" ? ProductType::GHand5 : ProductType::GHandLite1;
    auto config = LoadProductConfig(type);
    CHECK(config.valid_joints.size() == product["valid"].size());
    for (auto it = product["input_addresses"].begin(); it != product["input_addresses"].end(); ++it)
      CHECK(config.joint_input_registers.at(static_cast<JointId>(std::stoi(it.key()))) == it.value().get<int>());
    for (size_t i = 0; i < config.tactile_regions.size(); ++i)
      CHECK(config.tactile_regions[i].sensor_count == product["tactile_counts"][i].get<int>());
    for (auto it = product["limits"].begin(); it != product["limits"].end(); ++it) {
      auto limits = config.joint_limits.at(static_cast<JointId>(std::stoi(it.key())));
      CHECK(limits.first == it.value()[0].get<float>() && limits.second == it.value()[1].get<float>());
    }
    JointCommand command;
    command.id = static_cast<JointId>(product["first_joint"].get<int>());
    command.control_mode = JointControlMode::POSITION;
  command.control_state = JointControlState::RUNNING;
    command.loop_type = JointLoopType::SEMI_CLOSED_LOOP;
    command.angle = 30; command.velocity = -20; command.current = 40;
    command.kp = 7; command.kd = -8;
    auto registers = EncodeHandCommand({command}, HandControlMode::VELOCITY, config, {});
    auto expected_registers = product["command_registers"].get<std::vector<uint16_t>>();
    if (registers.size() != expected_registers.size())
      std::cerr << "register count: " << registers.size() << " != " << expected_registers.size() << '\n';
    for (size_t i = 0; i < std::min(registers.size(), expected_registers.size()); ++i) {
      if (registers[i] != expected_registers[i])
        std::cerr << "register " << i << ": " << registers[i] << " != " << expected_registers[i] << '\n';
    }
    CHECK(registers == product["command_registers"].get<std::vector<uint16_t>>());
    CHECK(EncodeEthercatCommand(config, HandControlMode::VELOCITY, true, {{command.id, command}}) == product["rpdo"].get<std::vector<uint8_t>>());
    command.control_mode = JointControlMode::VELOCITY;
    command.loop_type = JointLoopType::CLOSED_LOOP;
    CHECK(EncodeJointCommand(command)[0] == 0x0111);
    command.control_mode = JointControlMode::MIT;
    CHECK(EncodeJointCommand(command)[0] == 0x0113);
    auto ethercat = std::unique_ptr<EtherCATComm>(new EtherCATComm(config));
    int hand_calls = 0, joint_calls = 0, tactile_calls = 0;
    ethercat->SetHandStateCallback([&](uint16_t slave, const HandData& data, HandRuntimeState) {
      ++hand_calls; CHECK(data.temperature == -25);
      CHECK(type == ProductType::GHand5 ? data.error_fault[11] == 18 : data.error_fault[5] == 12);
    });
    ethercat->SetJointsCallback([&](uint16_t slave, const std::vector<JointData>& data) {
      ++joint_calls; CHECK(data.size() == config.valid_joints.size());
      CHECK(data[0].angle == -100 && data[0].velocity == -100);
      CHECK(data[0].control_mode == JointControlMode::MIT);
      CHECK(data[0].loop_type == JointLoopType::SEMI_CLOSED_LOOP);
    });
    ethercat->SetTactileDataCallback([&](uint16_t slave, const std::map<TactileSensorId, TactileData>& data) {
      ++tactile_calls;
      for (size_t i = 0; i < data.size(); ++i) {
        const auto& region = data.at(static_cast<TactileSensorId>(i));
        CHECK(region.resultant_force.at(0) == 25.6f);
        CHECK(region.distributed_force.front() == -1.0f);
        CHECK(region.distributed_force.back() == 3.0f);
      }
    });
    bytes = product["tpdo"].get<std::vector<uint8_t>>();
    ProtocolTestAccess::Parse(*ethercat, bytes);
    CHECK(hand_calls == 1 && joint_calls == 1 && tactile_calls == 1);
    bytes.pop_back();
    ProtocolTestAccess::Parse(*ethercat, bytes);
    CHECK(hand_calls == 1 && joint_calls == 1 && tactile_calls == 1);
    if (type == ProductType::GHandLite1) continue;
    bytes = product["tpdo"].get<std::vector<uint8_t>>();
    size_t thumb_samples = 28 + config.valid_joints.size() * 10 + 2 +
        6;
    bytes.erase(bytes.begin() + thumb_samples + 28 * 3,
                bytes.begin() + thumb_samples + config.tactile_regions[0].sensor_count * 3);
    ProtocolTestAccess::Parse(*ethercat, bytes);
    CHECK(hand_calls == 2 && joint_calls == 2 && tactile_calls == 2);
  }
}

void CheckPublicTypes() {
  CHECK(std::string(ToString(HandType::LEFT_HAND)) == "left_hand");
  CHECK(std::string(ToString(HandType::RIGHT_HAND)) == "right_hand");
  CHECK(std::string(ToString(ProductType::GHand5)) == "GHand5");
  CHECK(std::string(ToString(static_cast<CommType>(255))) == "unknown");
  JointCommand command;
  CHECK(command.id == JointId::THUMB_IP);
  CHECK(command.control_mode == JointControlMode::POSITION);
  CHECK(command.loop_type == JointLoopType::CLOSED_LOOP);
  CHECK(command.control_state == JointControlState::RUNNING);
  CHECK(EncodeJointCommand(command)[0] == 0x0110);
  JointCommand configured(JointId::THUMB_IP, JointControlMode::MIT,
                          JointLoopType::SEMI_CLOSED_LOOP,
                          JointControlState::STOPPED, 30.0f, 10, 20, 100, 5);
  CHECK(configured.control_state == JointControlState::STOPPED);
  CHECK(configured.control_mode == JointControlMode::MIT);
  CHECK(configured.loop_type == JointLoopType::SEMI_CLOSED_LOOP);
  CHECK(EncodeJointCommand(configured) ==
        std::vector<uint16_t>({0x0203, 300, 10, 20, 100, 5}));
  command.control_mode = JointControlMode::MIT;
  command.control_state = JointControlState::STOPPED;
  CHECK(EncodeJointCommand(command)[0] == 0x0213);
  uint16_t raw[] = {1, 0xF013, 0, 0, 0};
  auto joint = ParseJointData(raw, JointId::THUMB_IP);
  CHECK(joint.control_mode == JointControlMode::MIT);
  CHECK(joint.loop_type == JointLoopType::CLOSED_LOOP);
  auto faults = HandFault::ZEROING_BRAKE_TIMEOUT_LF3 |
      HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR12;
  CHECK(faults[8] == 1 && faults[9] == 0x20);
  CHECK((faults & HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR12) ==
      HandFault::ZEROING_MOTOR_COMMUNICATION_MOTOR12);
  HandData hand;
  hand.error_fault = faults;
  hand.error_fault = HandFault::NONE;
  hand.error_warn = HandWarning::FAN_ABNORMAL | HandWarning::SELF_TEST_ERROR;
  CHECK(hand.error_warn[0] == 4 && hand.error_warn[1] == 2);
  hand.error_critical = HandCriticalError::ZEROING_GLOBAL_TIMEOUT;
  CHECK(hand.error_critical[1] == 1);
  TactileData tactile;
  CHECK(!tactile.state && tactile.resultant_force.empty() && tactile.distributed_force.empty());
  DeviceData frame;
  CHECK(frame.tactile.empty() && !frame.timestamp && frame.joints.empty());
}

void CheckErrorCodes() {
  const JointErrorCode codes[] = {
      JointErrorCode::NORMAL, JointErrorCode::MOTOR_HARDWARE_OVERCURRENT,
      JointErrorCode::MOTOR_CONTINUOUS_CURRENT_HIGH, JointErrorCode::MOTOR_BUS_OVERCURRENT,
      JointErrorCode::MOTOR_HARDWARE_OVERVOLTAGE, JointErrorCode::MOTOR_HARDWARE_UNDERVOLTAGE,
      JointErrorCode::MOTOR_PHASE_LOSS, JointErrorCode::MOTOR_STALLED,
      JointErrorCode::MOTOR_CHIP_OVERTEMPERATURE, JointErrorCode::COMMUNICATION_LOST,
      JointErrorCode::MOTOR_SOFTWARE_OVERVOLTAGE, JointErrorCode::MOTOR_SOFTWARE_UNDERVOLTAGE,
      JointErrorCode::MOTOR_OVERTEMPERATURE, JointErrorCode::MOTOR_WINDING_OVERTEMPERATURE};
  const char* names[] = {"NORMAL", "MOTOR_HARDWARE_OVERCURRENT", "MOTOR_CONTINUOUS_CURRENT_HIGH",
      "MOTOR_BUS_OVERCURRENT", "MOTOR_HARDWARE_OVERVOLTAGE", "MOTOR_HARDWARE_UNDERVOLTAGE",
      "MOTOR_PHASE_LOSS", "MOTOR_STALLED", "MOTOR_CHIP_OVERTEMPERATURE", "COMMUNICATION_LOST",
      "MOTOR_SOFTWARE_OVERVOLTAGE", "MOTOR_SOFTWARE_UNDERVOLTAGE",
      "MOTOR_OVERTEMPERATURE", "MOTOR_WINDING_OVERTEMPERATURE"};
  for (size_t i = 0; i < 14; ++i) {
    CHECK(static_cast<uint8_t>(codes[i]) == i);
    CHECK(std::string(ToString(codes[i])) == names[i]);
    uint16_t raw[] = {static_cast<uint16_t>((i << 8) | 1), 0, 0, 0, 0};
    const auto joint = ParseJointData(raw, JointId::THUMB_MCP);
    CHECK(joint.error == codes[i]);
  }
  for (int i = 14; i <= 20; ++i)
    CHECK(std::string(ToString(static_cast<JointErrorCode>(i))) == "RESERVED");
  CHECK(std::string(ToString(static_cast<JointErrorCode>(21))) == "UNKNOWN_ERROR");


}

void CheckMailboxes() {
  int n = 0;
  CHECK(WaitCommandMailbox([&](uint16_t* value) { *value = n++ == 0 ? 1 : 0x100; return true; }, 0x100));
  CHECK(!WaitCommandMailbox([](uint16_t* value) { *value = 0x200; return true; }));
  CHECK(!WaitCommandMailbox([](uint16_t* value) { *value = 0x300; return true; }));
  CHECK(!WaitCommandMailbox([](uint16_t*) { return false; }, -1, 1));
  CHECK(WaitCommandMailbox([](uint16_t* value) { *value = 0x100; return true; }, 0x100));
}

void CheckPidParameters(IComm& comm, Simulator& node) {
  std::vector<uint16_t> expected;
  for (int i = 0; i < 39; ++i) expected.push_back(static_cast<uint16_t>(0x8100 + i));
  CHECK(comm.SetPidParameters(0x32, expected));
  CHECK(node.writes.back().address == 0x21 && node.writes.back().values[0] == 1);
  node.holding[0x315] = 0xFFFF;  // The read command must refresh the parameter window.
  std::vector<uint16_t> actual;
  CHECK(comm.GetPidParameters(0x32, &actual) && actual == expected);
  CHECK(node.writes.back().address == 0x21 && node.writes.back().values[0] == 2);
  CHECK(!comm.SetPidParameters(0x32, std::vector<uint16_t>(38)));
  CHECK(!comm.GetPidParameters(0, &actual));
  node.terminal = 0x200;
  CHECK(!comm.GetPidParameters(0x32, &actual) && actual == expected);
  node.terminal = 0x300;
  CHECK(!comm.SetPidParameters(0x32, expected));
  node.terminal = 0x100;
  node.fail_write = true;
  CHECK(!comm.SetPidParameters(0x32, expected));
  node.fail_write = false;

  std::map<JointId, uint8_t> tolerances;
  for (size_t i = 0; i < kStallToleranceJoints.size(); ++i)
    tolerances[kStallToleranceJoints[i]] = static_cast<uint8_t>(i + 1);
  const size_t first_write = node.writes.size();
  const auto holding = node.holding;
  std::map<JointId, uint8_t> actual_tolerances = tolerances;
  CHECK(!comm.SetStallTolerances(0x32, tolerances));
  CHECK(!comm.GetStallTolerances(0x32, &actual_tolerances));
  CHECK(actual_tolerances == tolerances);
  CHECK(node.writes.size() == first_write && node.holding == holding);
}

void CheckBackends(const json& vectors) {
  auto config = LoadProductConfig(ProductType::GHand5);
  SeedNode(serial_nodes[0x31], 0x31); SeedNode(serial_nodes[0x32], 0x32);
  JointCommand command(JointId::THUMB_MCP, JointControlMode::POSITION,
                       JointLoopType::CLOSED_LOOP, JointControlState::DISABLED,
                       30, -20, 40);
  command.control_mode = JointControlMode::POSITION;
  command.control_state = JointControlState::RUNNING;
  command.loop_type = JointLoopType::SEMI_CLOSED_LOOP; command.kp = 7; command.kd = -8;
  RS485Comm rs(config);
  CHECK(rs.Connect("simulated", 0, 5) == 0);
  CHECK(rs.GetConnectedHands().size() == 2);
  CHECK(rs.MoveJoints(0x31, {command}, HandControlMode::VELOCITY));
  CHECK(serial_nodes[0x31].writes.back().values == vectors["products"][0]["command_registers"].get<std::vector<uint16_t>>());
  CHECK(serial_nodes[0x32].writes.size() == 1);
  CheckPidParameters(rs, serial_nodes[0x32]);
  CHECK(!rs.SetSlaveId(0x31, 0x32));
  CHECK(rs.ClearFault(0x32)); CHECK(serial_nodes[0x32].writes.back().values[0] == 1);
  CHECK(rs.SetBaudrateConfig(0x32, 4)); CHECK(serial_nodes[0x32].writes.back().values[0] == 0x0401);
  serial_nodes[0x31].terminal = 0x200;
  CHECK(!rs.OpenTactile(0x31)); CHECK(rs.OpenTactile(0x32));
  CHECK(rs.GetHandType(0x31) == HandType::LEFT_HAND && rs.GetHandType(0x32) == HandType::RIGHT_HAND);
  CHECK(rs.Stop(0)); CHECK(rs.DisableHand(0));
  rs.Disconnect();
  CHECK(serial_nodes[0x31].writes.back().address == 0 && serial_nodes[0x32].writes.back().values[0] == 2);

  CANFDComm can(config);
  CHECK(can.Connect("simulated", 0, 5) == 0);
  CHECK(can.GetConnectedHands().size() == 2);
  CHECK(can.MoveJoints(0x31, {command}, HandControlMode::VELOCITY));
  CHECK(can_driver->nodes[0x31].writes.back().values == vectors["products"][0]["command_registers"].get<std::vector<uint16_t>>());
  CHECK(can_driver->nodes[0x32].writes.size() == 1);
  CheckPidParameters(can, can_driver->nodes[0x32]);
  CHECK(can.ClearFault(0x32)); CHECK(can_driver->nodes[0x32].writes.back().values[0] == 1);
  can_driver->nodes[0x31].terminal = 0x300;
  CHECK(!can.OpenTactile(0x31)); CHECK(can.OpenTactile(0x32));
  can_driver->nodes[0x31].fail_write = true;
  CHECK(!can.Stop(0)); CHECK(can_driver->nodes[0x32].writes.back().values[0] == 0x0200);
  can_driver->nodes[0x31].fail_write = false;
  can_driver->short_reply = true;
  std::vector<uint8_t> bytes;
  CHECK(ProtocolTestAccess::Read(can, 0x105F, 90, &bytes) && bytes.size() == 180);
  can.Disconnect();
  CHECK(can_driver->nodes[0x31].writes.back().address == 0 && can_driver->nodes[0x32].writes.back().values[0] == 2);
}

template <class Predicate>
void WaitFor(Predicate predicate) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (!predicate() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  CHECK(predicate());
}

void CheckPublicApi() {
  for (CommType type : {CommType::CANFD, CommType::RS485}) {
    serial_nodes.clear(); SeedNode(serial_nodes[0x31], 0x31); SeedNode(serial_nodes[0x32], 0x32);
    std::atomic<int> left_calls{0}, right_calls{0};
    auto master = Master::Create(ProductType::GHand5, type);
    CHECK(master);
    auto connected = master->Connect("simulated");
    CHECK(connected && connected.value.size() == 2 && master->hands().size() == 2);
    auto left = master->hand(HandType::LEFT_HAND); auto right = master->hand(HandType::RIGHT_HAND);
    CHECK(left && right && left != right);
    JointCommand invalid;
    invalid.id = JointId::THUMB_MCP;
    invalid.control_mode = static_cast<JointControlMode>(0x0F);
    CHECK(left->MoveJoints({invalid}).code == SdkError::INVALID_ARGUMENT);
    invalid.control_mode = JointControlMode::POSITION;
    invalid.loop_type = static_cast<JointLoopType>(2);
    CHECK(left->MoveJoints({invalid}).code == SdkError::INVALID_ARGUMENT);
    invalid.loop_type = JointLoopType::CLOSED_LOOP;
    invalid.control_state = static_cast<JointControlState>(3);
    CHECK(left->MoveJoints({invalid}).code == SdkError::INVALID_ARGUMENT);
    std::vector<uint16_t> pid(39, 123);
    CHECK(left->SetPidParameters(pid).code == SdkError::NOT_SUPPORTED);
    CHECK(left->GetPidParameters().code == SdkError::NOT_SUPPORTED);
    std::map<JointId, uint8_t> tolerances;
    for (JointId id : kStallToleranceJoints) tolerances[id] = 7;
    CHECK(left->SetStallTolerances(tolerances).code == SdkError::NOT_SUPPORTED);
    CHECK(left->GetStallTolerances().code == SdkError::NOT_SUPPORTED);
    left->SetHandStateCallback([&](const HandData& state) { if (state.temperature == 20) ++left_calls; });
    right->SetHandStateCallback([&](const HandData& state) { if (state.temperature == 30) ++right_calls; });
    WaitFor([&] { return left_calls > 0 && right_calls > 0; });
    CHECK(left->GetHandData().value.temperature == 20 && right->GetHandData().value.temperature == 30);
    std::atomic<int> joint_calls{0}, tactile_calls{0};
    left->SetJointsCallback([&](const std::vector<JointData>&) { ++joint_calls; });
    left->SetTactileDataCallback([&](const std::map<TactileSensorId, TactileData>&) { ++tactile_calls; });
    CHECK(left->OpenTactile());
    WaitFor([&] { return joint_calls > 0 && tactile_calls > 0; });
    auto set_machine = [&](uint16_t machine) {
      if (type == CommType::CANFD) {
        std::lock_guard<std::mutex> lock(can_driver->mutex);
        can_driver->nodes[0x31].inputs[0x1048] = machine;
      } else {
        std::lock_guard<std::mutex> lock(serial_mutex);
        serial_nodes[0x31].inputs[0x1048] = machine;
      }
    };
    left->SetHandStateCallback([&](const HandData& state) {
      if (state.temperature == 20) ++left_calls;
    });
    set_machine(1);
    WaitFor([&] { return left->GetHandData().code == SdkError::DEVICE_REJECTED; });
    CHECK(left->GetHandData().code == SdkError::DEVICE_REJECTED);
    const auto self_test_joints = left->GetJointsData();
    const auto self_test_tactile = left->GetTactileData();
    CHECK(self_test_joints.code == SdkError::DEVICE_REJECTED && self_test_joints.value.empty());
    CHECK(self_test_tactile.code == SdkError::DEVICE_REJECTED && self_test_tactile.value.empty());
    CHECK(right->GetHandData() && right->GetJointsData() && right->GetTactileData());
    int paused_joints = joint_calls, paused_tactile = tactile_calls;
    int state_calls = left_calls;
    int other_hand_calls = right_calls;
    WaitFor([&] { return right_calls > other_hand_calls; });
    CHECK(left_calls == state_calls && joint_calls == paused_joints && tactile_calls == paused_tactile);
    set_machine(2);
    WaitFor([&] { return joint_calls > paused_joints && tactile_calls > paused_tactile; });
    WaitFor([&] { return left->GetHandData().code == SdkError::OK; });
    CHECK(left->GetJointsData() && !left->GetJointsData().value.empty());
    CHECK(left->GetTactileData() && !left->GetTactileData().value.empty());
    CHECK(left->CloseTactile());
    left->SetJointsCallback(nullptr);
    left->SetTactileDataCallback(nullptr);
    left->SetHandStateCallback([&](const HandData& state) { if (state.temperature == 20) ++left_calls; });
    CHECK(left->SetSlaveId(0x31));
    int idempotent_calls = left_calls;
    WaitFor([&] { return left_calls > idempotent_calls; });
    JointCommand command(JointId::THUMB_MCP, JointControlMode::POSITION,
                       JointLoopType::CLOSED_LOOP, JointControlState::DISABLED,
                       30, 20, 40);
    CHECK(left->MoveJoints({command}, HandControlMode::VELOCITY));
    CHECK(right->MoveJoints({command}, HandControlMode::CURRENT));
    command.control_mode = JointControlMode::VELOCITY;
    CHECK(left->MoveJoints({command}, HandControlMode::CUSTOM));
    command.control_mode = JointControlMode::CURRENT;
    CHECK(right->MoveJoints({command}, HandControlMode::CUSTOM));
    if (type == CommType::CANFD) {
      std::lock_guard<std::mutex> lock(can_driver->mutex);
      CHECK((can_driver->nodes[0x31].writes.back().values[1] & 0x0F) == 1);
      CHECK((can_driver->nodes[0x32].writes.back().values[1] & 0x0F) == 2);
    } else {
      std::lock_guard<std::mutex> lock(serial_mutex);
      CHECK((serial_nodes[0x31].writes.back().values[1] & 0x0F) == 1);
      CHECK((serial_nodes[0x32].writes.back().values[1] & 0x0F) == 2);
    }
    CHECK(left->Stop()); CHECK(right->Stop());
    CHECK(left->SetSlaveId(0x33)); CHECK(master->hand(static_cast<uint16_t>(0x31)) == nullptr);
    CHECK(master->hand(static_cast<uint16_t>(0x33)) == left);
    int previous = left_calls;
    WaitFor([&] { return left_calls > previous; });
    CHECK(right->GetHandData().value.temperature == 30);
    if (type == CommType::CANFD) {
      auto slow_callback_entered = std::make_shared<std::atomic<bool>>(false);
      right->SetHandStateCallback([slow_callback_entered](const HandData&) {
        *slow_callback_entered = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(350));
      });
      WaitFor([&] { return slow_callback_entered->load(); });
      int reads = can_driver->keepalive_reads;
      WaitFor([&] { return can_driver->keepalive_reads > reads; });
      // Restore before the local slow-callback flag leaves scope.
      right->SetHandStateCallback([&](const HandData& state) { if (state.temperature == 30) ++right_calls; });
    }
    if (type == CommType::CANFD) can_driver->nodes[0x33].online = false;
    else serial_nodes[0x33].online = false;
    CHECK(!left->MoveJoints({command}));
    CHECK(right->MoveJoints({command}));
    previous = right_calls;
    WaitFor([&] { return right_calls > previous; });
    CHECK(left->GetHandData().value.temperature == 20);
    if (type == CommType::CANFD) can_driver->nodes[0x33].online = true;
    else serial_nodes[0x33].online = true;
    CHECK(master->Disconnect()); CHECK(master->hands().empty());
    connected = master->Connect("simulated", 0x32);
    CHECK(connected && connected.value.size() == 1 && master->hand(HandType::LEFT_HAND) == nullptr);
    if (type == CommType::CANFD) can_driver->fail_open = true;
    else serial_connect_fail = true;
    CHECK(!master->Connect("simulated", 0x32));
    CHECK(master->hands().empty() && !master->IsConnected());
    serial_connect_fail = false;
    CHECK(master->Disconnect());
  }
}

void CheckEthercatBus(const json& vectors) {
  auto config = LoadProductConfig(ProductType::GHand5);
  auto bus = std::unique_ptr<EtherCATComm, void(*)(EtherCATComm*)>(new EtherCATComm(config), ProtocolTestAccess::CleanupEthercat);
  ProtocolTestAccess::SeedEthercat(*bus);
  CHECK(bus->GetDeviceInfo(2).serial_number == "1234567890123456789");
  HandRuntimeState state;
  CHECK(bus->GetHandStateMachine(2, &state) && state == HandRuntimeState::IDLE);
  std::vector<uint16_t> pid;
  CHECK(bus->GetPidParameters(2, &pid) && pid.size() == 39 && pid.front() == 0x0201);
  std::map<JointId, uint8_t> tolerances;
  CHECK(bus->GetStallTolerances(2, &tolerances) && tolerances.size() == 13);
  CHECK(tolerances.at(JointId::THUMB_MCP) == 1 && tolerances.at(JointId::LF_MCP) == 13);
  JointCommand command(JointId::THUMB_MCP, JointControlMode::POSITION,
                       JointLoopType::CLOSED_LOOP, JointControlState::DISABLED,
                       10, 20, 30);
  CHECK(bus->MoveJoints(2, {command}, HandControlMode::VELOCITY));
  command.angle = 40;
  CHECK(bus->MoveJoints(3, {command}, HandControlMode::CURRENT));
  auto left = ProtocolTestAccess::Output(*bus, 2), right = ProtocolTestAccess::Output(*bus, 3);
  CHECK(left != right && left.size() == 158 && right.size() == 158);
  CHECK(left[0] == 1 && right[0] == 2);
  CHECK(!bus->MoveJoints(1, {command}, HandControlMode::POSITION));
  CHECK(bus->Stop(0)); CHECK(bus->DisableHand(0));
  CHECK(ProtocolTestAccess::Output(*bus, 2)[1] == 0 && ProtocolTestAccess::Output(*bus, 3)[1] == 0);
  std::map<uint16_t, int> calls;
  bus->SetHandStateCallback([&](uint16_t slave, const HandData&, HandRuntimeState) { ++calls[slave]; });
  auto bytes = vectors["products"][0]["tpdo"].get<std::vector<uint8_t>>();
  ProtocolTestAccess::ParseSlave(*bus, bytes, 2); ProtocolTestAccess::ParseSlave(*bus, bytes, 3);
  CHECK(calls[2] == 1 && calls[3] == 1 && calls.count(1) == 0);
}

int main() {
  try {
    std::ifstream input("tests/python_protocol_vectors.json");
    CHECK(input.good());
    json vectors; input >> vectors;
    CheckVectors(vectors);
    CheckPublicTypes();
    CheckErrorCodes();
    CheckMailboxes();
    CheckBackends(vectors);
    CheckPublicApi();
    CheckEthercatBus(vectors);
    std::cout << "Protocol vectors, mailboxes and simulated backends passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
