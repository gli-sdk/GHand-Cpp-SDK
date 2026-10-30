// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only
#ifndef SRC_TACTILE_DISPLAY_TACTILE_ACQUISITION_H_
#define SRC_TACTILE_DISPLAY_TACTILE_ACQUISITION_H_

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include "tactile_display/tactile_widgets.h"

using Frame = std::map<ghand::TactileSensorId, ghand::TactileData>;
inline double TactileNow() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline FingerForces InvalidForces() {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  FingerForces forces; forces.fill({{nan,nan,nan}}); return forces;
}

// Same 50 Hz buckets, bounded queue, generation checks and 60-second retention
// as Python's _TactileDataStore. UI refresh does not manufacture sensor samples.
struct Acquisition {
  std::mutex mutex;
  Frame frame;
  ForceHistory pending, history;
  unsigned generation = 0;

  bool Ingest(const Frame& incoming, unsigned token, double now) {
    ForceSample sample{now,InvalidForces()};
    Frame distributed;
    for (const auto& entry : incoming) {
      const int finger = static_cast<int>(entry.first);
      const auto& data = entry.second;
      if (finger < 0 || finger >= 5 || !data.state) continue;
      if (data.resultant_force.size() == 3 &&
          std::all_of(data.resultant_force.begin(),data.resultant_force.end(),[](float v) { return std::isfinite(v); }))
        std::copy_n(data.resultant_force.begin(),3,sample.forces[finger].begin());
      if (data.distributed_force.size() == size_t((finger == 0 ? 38 : 31)*3) &&
          std::all_of(data.distributed_force.begin(),data.distributed_force.end(),[](float v) { return std::isfinite(v); }))
        distributed.emplace(entry);
    }
    std::lock_guard<std::mutex> lock(mutex);
    if (token != generation) return false;
    const auto bucket = [](double time) { return static_cast<int64_t>(time*50+1e-7); };
    if (!pending.empty() && bucket(pending.back().time) == bucket(now)) pending.back() = sample;
    else { pending.push_back(sample); if (pending.size() > 3100) pending.pop_front(); }
    frame = std::move(distributed);
    return true;
  }
  void Clear(bool clear_history) {
    std::lock_guard<std::mutex> lock(mutex);
    ++generation; pending.clear(); frame.clear();
    if (clear_history) history.clear();
  }
  void Drain(double now, Frame* distributed, ForceHistory* samples) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!history.empty() && !pending.empty() && pending.front().time-history.back().time > .5)
      history.push_back({history.back().time+.5,InvalidForces()});
    history.insert(history.end(),pending.begin(),pending.end()); pending.clear();
    if (!history.empty() && now-history.back().time > .5)
      history.push_back({history.back().time+.5,InvalidForces()});
    while (!history.empty() && history.front().time < now-60) history.pop_front();
    while (history.size() > 3100) history.pop_front();
    *distributed = frame; *samples = history;
  }
};

#endif
