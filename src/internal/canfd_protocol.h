#ifndef SRC_INTERNAL_CANFD_PROTOCOL_H_
#define SRC_INTERNAL_CANFD_PROTOCOL_H_

#include <chrono>
#include <cstdint>
#include <vector>

namespace xiaoyao {
namespace internal {
namespace canfd {

// 帧类型（仲裁域 bit[28:25]）
enum FrameType : uint8_t {
    BROADCAST     = 0x0,
    COMMAND       = 0x1,
    RESPONSE      = 0x2,
    ACTIVE_REPORT = 0x3,
};

// 功能码（数据域首字节）
enum FunctionCode : uint8_t {
    GET_DEVICE_NAME     = 0x10,
    GET_HW_VERSION      = 0x11,
    GET_SW_VERSION      = 0x12,
    GET_SERIAL          = 0x13,
    GET_HAND_TYPE       = 0x14,
    GET_BOARD_STATE     = 0x15,
    GET_BOARD_ERROR     = 0x16,
    GET_TEMPERATURE     = 0x17,
    GET_TACTILE_SINGLE  = 0x18,
    GET_ALL_JOINTS      = 0x19,
    GET_TACTILE_FORCE   = 0x1A,
    GET_SLAVE_ID        = 0x1B,
    GET_COMM_PARAMS     = 0x1C,
    CLEAR_FAULT         = 0x50,
    INIT_JOINT          = 0x51,
    SET_BAUDRATE        = 0x52,
    SET_SLAVE_ID        = 0x53,
    CONTROL_JOINTS      = 0x54,
    ZERO_TACTILE        = 0x55,
    OPEN_TACTILE        = 0x56,
    CLOSE_TACTILE       = 0x57,
    REPORT_ERROR        = 0x90,
    REPORT_JOINTS       = 0x91,
    REPORT_TACTILE      = 0x92,
};

// 29-bit 仲裁域编码/解码
struct ArbitrationId {
    uint32_t raw = 0;

    ArbitrationId() = default;
    explicit ArbitrationId(uint32_t id) : raw(id) {}

    /**
     * @brief 构造 29-bit 扩展帧仲裁域（CAN ID）
     *
     * 位域分布（从高位到低位）：
     *   bit[28:25]  FrameType  (4bit)  帧类型：COMMAND/RESPONSE/ACTIVE_REPORT/BROADCAST
     *   bit[24:17]  device_id  (8bit)  目标从机地址（如 0x71）
     *   bit[16:13]  seq        (4bit)  帧索引（多帧组包时的第几片，单帧为 0）
     *   bit[12:9 ]  total      (4bit)  总分片数（单帧为 1，多帧组包时 >1）
     *   bit[8:0  ]            (9bit)  保留未用
     *
     * 将元数据嵌入 ID 可利用 CAN 硬件滤波器，减少数据域开销。
     */
    ArbitrationId(FrameType ft, uint8_t dev_id, uint8_t seq, uint8_t total) {
        raw = 0;
        raw |= (static_cast<uint32_t>(ft) & 0xF) << 25;
        raw |= (static_cast<uint32_t>(dev_id) & 0xFF) << 17;
        raw |= (static_cast<uint32_t>(seq) & 0xF) << 13;
        raw |= (static_cast<uint32_t>(total) & 0xF) << 9;
    }

    FrameType frame_type() const { return static_cast<FrameType>((raw >> 25) & 0xF); }
    uint8_t device_id() const    { return static_cast<uint8_t>((raw >> 17) & 0xFF); }
    uint8_t seq() const          { return static_cast<uint8_t>((raw >> 13) & 0xF); }
    uint8_t total() const        { return static_cast<uint8_t>((raw >> 9) & 0xF); }
};

// CANFD 单帧数据
struct Frame {
    uint32_t id = 0;          // 29-bit 扩展帧仲裁域（CAN ID），由 ArbitrationId 打包帧类型/设备ID/序列号
    uint8_t data[64] = {0};   // 数据域，CANFD 最大 64 字节
    uint8_t len = 0;          // 实际数据长度，必须为 CANFD 合法长度之一（0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64）
    bool is_fd = true;        // true=CANFD 格式，false=普通 CAN 格式
    bool is_extended = true;  // true=扩展帧（29-bit ID），false=标准帧（11-bit ID）
};

// 多帧包组装器
class PacketAssembler {
public:
    bool Feed(const Frame& frame, std::vector<uint8_t>* out_payload);
    void Reset();

private:
    std::vector<uint8_t> buffer_;
    uint8_t expected_total_ = 0;
    uint16_t received_mask_ = 0;
    std::chrono::steady_clock::time_point first_frame_time_;
    static constexpr auto kAssemblyTimeout = std::chrono::milliseconds(500);
};

} // namespace canfd
} // namespace internal
} // namespace xiaoyao

#endif // SRC_INTERNAL_CANFD_PROTOCOL_H_
