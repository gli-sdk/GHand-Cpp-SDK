#include "canfd_protocol.h"
#include <cstring>

namespace xiaoyao {
namespace internal {
namespace canfd {

bool PacketAssembler::Feed(const Frame& frame, std::vector<uint8_t>* out_payload) {
    if (!out_payload) return false;

    ArbitrationId arb(frame.id);
    uint8_t seq = arb.seq();
    uint8_t total = arb.total();

    auto now = std::chrono::steady_clock::now();

    // 超时检测：正在组包且超过 500ms 未收齐，自动重置
    if (expected_total_ != 0 &&
        (now - first_frame_time_) > kAssemblyTimeout) {
        Reset();
    }

    // 单帧包（兼容 total=0 或 total=1，与发送端保持一致）
    if (total <= 1) {
        out_payload->assign(frame.data, frame.data + frame.len);
        return true;
    }

    if (expected_total_ == 0) {
        expected_total_ = total;
        buffer_.clear();
        received_mask_ = 0;
        first_frame_time_ = now;
    }

    if (total != expected_total_) {
        // 收到不同总帧数的包，重置
        Reset();
        expected_total_ = total;
        first_frame_time_ = now;
    }

    if (seq >= total) {
        return false;
    }

    if (received_mask_ & (1 << seq)) {
        // 重复帧，忽略
        return false;
    }

    size_t offset = seq * 64;
    if (buffer_.size() < offset + frame.len) {
        buffer_.resize(offset + frame.len);
    }
    memcpy(buffer_.data() + offset, frame.data, frame.len);
    received_mask_ |= (1 << seq);

    // 检查是否收齐
    uint16_t expected_mask = (1 << total) - 1;
    if (received_mask_ == expected_mask) {
        *out_payload = buffer_;
        Reset();
        return true;
    }

    return false;
}

void PacketAssembler::Reset() {
    buffer_.clear();
    expected_total_ = 0;
    received_mask_ = 0;
    first_frame_time_ = std::chrono::steady_clock::time_point{};
}

} // namespace canfd
} // namespace internal
} // namespace xiaoyao
