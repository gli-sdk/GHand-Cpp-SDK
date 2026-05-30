#include "canfd_protocol.h"

#include <cstring>

namespace ghand {
namespace internal {
namespace canfd {

// C++11: static constexpr non-integral member needs out-of-line definition
constexpr decltype(PacketAssembler::kAssemblyTimeout)
    PacketAssembler::kAssemblyTimeout;

bool PacketAssembler::Feed(const Frame& frame,
                           std::vector<uint8_t>* out_payload) {
  if (!out_payload) return false;

  ArbitrationId arb(frame.id);
  uint8_t seq = arb.seq();
  uint8_t total = arb.total();

  auto now = std::chrono::steady_clock::now();

  // Timeout detection: if assembly is in progress and not completed within 500ms, auto-reset
  if (expected_total_ != 0 && (now - first_frame_time_) > kAssemblyTimeout) {
    Reset();
  }

  // Single-frame packet (compatible with total=0 or total=1, consistent with sender)
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
    // Received packet with different total frame count, reset
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
    // Duplicate frame, ignore
    return false;
  }

  frame_offsets_[seq] = static_cast<uint16_t>(buffer_.size());
  frame_lens_[seq] = frame.len;
  buffer_.insert(buffer_.end(), frame.data, frame.data + frame.len);
  received_mask_ |= (1 << seq);

  // Check if all frames received
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
