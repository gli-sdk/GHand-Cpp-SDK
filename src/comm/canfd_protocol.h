// Copyright 2025 Glitech.

#ifndef SRC_COMM_CANFD_PROTOCOL_H_
#define SRC_COMM_CANFD_PROTOCOL_H_

#include <chrono>
#include <cstdint>
#include <vector>

namespace ghand {
namespace internal {
namespace canfd {

// Frame type (arbitration field bits [28:25])
enum FrameType : uint8_t {
  BROADCAST = 0x0,
  COMMAND = 0x1,
  RESPONSE = 0x2,
  ACTIVE_REPORT = 0x3,
};

// Function code (first byte of data field)
enum FunctionCode : uint8_t {
  GET_DEVICE_NAME = 0x10,
  GET_HW_VERSION = 0x11,
  GET_SW_VERSION = 0x12,
  GET_SERIAL = 0x13,
  GET_HAND_TYPE = 0x14,
  GET_BOARD_STATE = 0x15,
  GET_BOARD_ERROR = 0x16,
  GET_TEMPERATURE = 0x17,
  GET_TACTILE_SINGLE = 0x18,
  GET_ALL_JOINTS = 0x19,
  GET_TACTILE_FORCE = 0x1A,
  GET_SLAVE_ID = 0x1B,
  GET_COMM_PARAMS = 0x1C,
  CLEAR_FAULT = 0x50,
  INIT_JOINT = 0x51,
  SET_BAUDRATE = 0x52,
  SET_SLAVE_ID = 0x53,
  CONTROL_JOINTS = 0x54,
  ZERO_TACTILE = 0x55,
  OPEN_TACTILE = 0x56,
  CLOSE_TACTILE = 0x57,
  REPORT_ERROR = 0x90,
  REPORT_JOINTS = 0x91,
  REPORT_TACTILE = 0x92,
};

// 29-bit arbitration field encoding/decoding
struct ArbitrationId {
  uint32_t raw = 0;

  ArbitrationId() = default;
  explicit ArbitrationId(uint32_t id) : raw(id) {}

  /**
   * @brief Construct a 29-bit extended frame arbitration field (CAN ID)
   *
   * Bit field layout (from MSB to LSB):
   *   bit[28:25]  FrameType  (4bit)
   * Frame type: COMMAND/RESPONSE/ACTIVE_REPORT/BROADCAST
   *   bit[24:17]  device_id  (8bit)  Target slave address (e.g. 0x71)
   *   bit[16:13]  seq        (4bit)  Frame index in multi-frame assembly
   *                                      (0 for single frame)
   *   bit[12:9 ]  total      (4bit)  Total number of fragments
   *                                      (1 for single frame,
   *                                      >1 for multi-frame)
   *   bit[8:0  ]            (9bit)  Reserved/unused
   *
   * Embedding metadata in the ID allows using CAN hardware filters and
   * reduces data field overhead.
   */
  ArbitrationId(FrameType ft, uint8_t dev_id, uint8_t seq, uint8_t total) {
    raw = 0;
    raw |= (static_cast<uint32_t>(ft) & 0xF) << 25;
    raw |= (static_cast<uint32_t>(dev_id) & 0xFF) << 17;
    raw |= (static_cast<uint32_t>(seq) & 0xF) << 13;
    raw |= (static_cast<uint32_t>(total) & 0xF) << 9;
  }

  FrameType frame_type() const {
    return static_cast<FrameType>((raw >> 25) & 0xF);
  }
  uint8_t device_id() const {
    return static_cast<uint8_t>((raw >> 17) & 0xFF);
  }
  uint8_t seq() const { return static_cast<uint8_t>((raw >> 13) & 0xF); }
  uint8_t total() const { return static_cast<uint8_t>((raw >> 9) & 0xF); }
};

// CANFD single frame data
struct Frame {
  // 29-bit extended frame arbitration field (CAN ID), packed with frame
  // type/device ID/sequence by ArbitrationId.
  uint32_t id = 0;
  uint8_t data[64] = {0};  // Data field, CANFD max 64 bytes
  // Actual data length, must be one of the CANFD valid lengths:
  // 0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64.
  uint8_t len = 0;
  bool is_fd = true;  // true=CANFD format, false=standard CAN format
  // true=extended frame (29-bit ID), false=standard frame (11-bit ID)
  bool is_extended = true;
};

// Multi-frame packet assembler
class PacketAssembler {
 public:
  bool Feed(const Frame& frame, std::vector<uint8_t>* out_payload);
  void Reset();

 private:
  std::vector<uint8_t> buffer_;
  std::vector<uint16_t> frame_offsets_;
  std::vector<uint8_t> frame_lens_;
  uint8_t expected_total_ = 0;
  uint16_t received_mask_ = 0;
  std::chrono::steady_clock::time_point first_frame_time_;
  static constexpr auto kAssemblyTimeout = std::chrono::milliseconds(500);
};

}  // namespace canfd
}  // namespace internal
}  // namespace ghand

#endif  // SRC_COMM_CANFD_PROTOCOL_H_
