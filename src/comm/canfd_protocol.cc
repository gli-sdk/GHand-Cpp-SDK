#include "canfd_protocol.h"

#include <cstring>

namespace ghand {
namespace internal {
namespace canfd {

bool PacketAssembler::Feed(const Frame& frame,
                           std::vector<uint8_t>* out_payload) {
  if (!out_payload) return false;

  ArbitrationId arb(frame.id);
  uint8_t seq = arb.seq();
  uint8_t total = arb.total();

  auto now = std::chrono::steady_clock::now();

  // 超时检测：正在组包且超过 500ms 未收齐，自动重置
  if (expected_total_ != 0 && (now - first_frame_time_) > kAssemblyTimeout) {
    Reset();
  }

  // 单帧包（兼容 total=0 或 total=1，与发送端保持一致）
  if (total <= 1) {
    out_payload->assign(frame.data, frame.data + frame.len);
    return true;
  }

  if (expected_total_ == 0) {
    expected_total_ = total;
    frame_offsets_.assign(total, 0);
    frame_lens_.assign(total, 0);
    buffer_.clear();
    received_mask_ = 0;
    first_frame_time_ = now;
  }

  if (total != expected_total_) {
    // 收到不同总帧数的包，重置
    Reset();
    expected_total_ = total;
    frame_offsets_.assign(total, 0);
    frame_lens_.assign(total, 0);
    buffer_.clear();
    first_frame_time_ = now;
  }

  if (seq >= total) {
    return false;
  }

  if (received_mask_ & (1 << seq)) {
    // 重复帧，忽略
    return false;
  }

  frame_offsets_[seq] = static_cast<uint16_t>(buffer_.size());
  frame_lens_[seq] = frame.len;
  buffer_.insert(buffer_.end(), frame.data, frame.data + frame.len);
  received_mask_ |= (1 << seq);

  // 检查是否收齐
  uint16_t expected_mask = (1U << total) - 1;
  if (received_mask_ == expected_mask) {
    out_payload->clear();
    for (uint8_t i = 0; i < total; ++i) {
      uint16_t off = frame_offsets_[i];
      uint8_t len = frame_lens_[i];
      out_payload->insert(out_payload->end(), buffer_.begin() + off,
                          buffer_.begin() + off + len);
    }
    Reset();
    return true;
  }

  return false;
}

void PacketAssembler::Reset() {
  buffer_.clear();
  frame_offsets_.clear();
  frame_lens_.clear();
  expected_total_ = 0;
  received_mask_ = 0;
  first_frame_time_ = std::chrono::steady_clock::time_point{};
}

}  // namespace canfd
}  // namespace internal
}  // namespace ghand
