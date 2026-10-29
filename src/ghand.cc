// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ghand/ghand.h"

#include "internal/ghand_impl.h"

namespace ghand {

namespace internal {

// Hands are only created through Master; this factory keeps that constructor
// reachable for Master's helper without widening GHand's public surface.
struct HandFactory {
  static std::unique_ptr<GHand> Make(GHandImpl* impl, uint16_t slave_index,
                                     HandType hand_type) {
    return std::unique_ptr<GHand>(
        new GHand(impl, slave_index, hand_type));
  }
};

}  // namespace internal

namespace {

// Turn a connect result into the hand objects the session owns.
ResultOf<std::vector<GHand*>> AdoptHands(
    internal::GHandImpl* impl, std::vector<std::unique_ptr<GHand>>* storage,
    const ResultOf<std::vector<internal::ConnectedHand>>& connected) {
  if (!connected) return {connected.code, connected.message};

  storage->clear();
  std::vector<GHand*> hands;
  hands.reserve(connected.value.size());
  for (const auto& hand : connected.value) {
    storage->push_back(internal::HandFactory::Make(
        impl, hand.slave_index, hand.hand_type));
    hands.push_back(storage->back().get());
  }
  return hands;
}

}  // namespace

// ---------------------------------------------------------------------------
// GHand
// ---------------------------------------------------------------------------

GHand::GHand(internal::GHandImpl* impl, uint16_t slave_index,
             HandType hand_type)
    : impl_(impl),
      slave_index_(slave_index),
      hand_type_(hand_type) {}

GHand::~GHand() {}

ResultOf<DeviceInfo> GHand::GetDeviceInfo() {
  return impl_->GetDeviceInfo(slave_index_);
}

Result GHand::MoveJoints(const std::vector<JointCommand>& joints,
                         HandControlMode mode) {
  return impl_->MoveJoints(slave_index_, joints, mode);
}

Result GHand::Stop() { return impl_->Stop(slave_index_); }

Result GHand::ClearFault() { return impl_->ClearFault(slave_index_); }

Result GHand::OpenTactile() { return impl_->OpenTactile(slave_index_); }

Result GHand::CloseTactile() { return impl_->CloseTactile(slave_index_); }

Result GHand::ZeroTactile() { return impl_->ZeroTactile(slave_index_); }

Result GHand::SetSlaveId(uint8_t new_slave_id) {
  Result result = impl_->SetSlaveId(slave_index_, new_slave_id);
  if (result) slave_index_ = new_slave_id;  // the hand answers to its new ID
  return result;
}

Result GHand::SetBaudrateConfig(RS485BaudRate baud_rate) {
  return impl_->SetBaudrateConfig(slave_index_, baud_rate);
}

Result GHand::SetBaudrateConfig(CANFDBitTiming timing) {
  return impl_->SetBaudrateConfig(slave_index_, timing);
}

void GHand::SetJointsCallback(JointsCallback cb) {
  impl_->SetJointsCallback(slave_index_, cb);
}

void GHand::SetHandStateCallback(HandStateCallback cb) {
  impl_->SetHandStateCallback(slave_index_, cb);
}

void GHand::SetTactileDataCallback(TactileDataCallback cb) {
  impl_->SetTactileDataCallback(slave_index_, cb);
}

ResultOf<HandData> GHand::GetHandData() {
  return impl_->GetHandData(slave_index_);
}

ResultOf<std::vector<JointData>> GHand::GetJointsData() {
  return impl_->GetJointsData(slave_index_);
}

ResultOf<std::map<TactileSensorId, TactileData>> GHand::GetTactileData() {
  return impl_->GetTactileData(slave_index_);
}

Result GHand::DisableHand() { return impl_->DisableHand(slave_index_); }

ResultOf<HandRuntimeState> GHand::GetHandStateMachine() { return impl_->GetHandStateMachine(slave_index_); }

ResultOf<std::map<JointId, uint8_t>> GHand::GetStallTolerances() { return impl_->GetStallTolerances(slave_index_); }

Result GHand::SetStallTolerances(const std::map<JointId, uint8_t>& values) { return impl_->SetStallTolerances(slave_index_, values); }

ResultOf<std::vector<uint16_t>> GHand::GetPidParameters() { return impl_->GetPidParameters(slave_index_); }

Result GHand::SetPidParameters(const std::vector<uint16_t>& values) { return impl_->SetPidParameters(slave_index_, values); }

FirmwareUpdateError GHand::BootUpdate(
    const std::string& filename,
    std::function<void(int)> progress_callback) {
  return impl_->BootUpdate(slave_index_, filename, progress_callback);
}

// ---------------------------------------------------------------------------
// Master
// ---------------------------------------------------------------------------

std::unique_ptr<Master> Master::Create(ProductType pt, CommType ct) {
  std::unique_ptr<Master> master(new Master(pt, ct));
  if (!master->impl_->IsValid()) {
    return nullptr;
  }
  return master;
}

Master::Master(ProductType pt, CommType ct)
    : impl_(new internal::GHandImpl(pt, ct)) {}

Master::~Master() { impl_->Disconnect(); hands_.clear(); }

ResultOf<std::vector<GHand*>> Master::Connect(const std::string& device_name,
                                              uint8_t slave_id) {
  impl_->Disconnect();
  hands_.clear();
  return AdoptHands(impl_.get(), &hands_, impl_->Connect(device_name, slave_id));
}

ResultOf<std::vector<GHand*>> Master::Connect(const std::string& device_name,
                                              uint8_t slave_id,
                                              RS485BaudRate baud_rate) {
  impl_->Disconnect();
  hands_.clear();
  return AdoptHands(impl_.get(), &hands_,
                    impl_->Connect(device_name, slave_id, baud_rate));
}

ResultOf<std::vector<GHand*>> Master::Connect(const std::string& device_name,
                                              uint8_t slave_id,
                                              CANFDBitTiming timing) {
  impl_->Disconnect();
  hands_.clear();
  return AdoptHands(impl_.get(), &hands_,
                    impl_->Connect(device_name, slave_id, timing));
}

Result Master::Disconnect() { auto result = impl_->Disconnect(); hands_.clear(); return result; }

bool Master::IsConnected() const { return impl_->IsConnected(); }

std::map<std::string, std::string> Master::SearchAdapters() {
  return impl_->SearchAdapters();
}

std::vector<GHand*> Master::hands() const {
  std::vector<GHand*> hands;
  hands.reserve(hands_.size());
  for (const auto& hand : hands_) {
    hands.push_back(hand.get());
  }
  return hands;
}

GHand* Master::hand(HandType type) const {
  if (type == HandType::NONE) return nullptr;
  for (const auto& hand : hands_) {
    if (hand->GetHandType() == type) return hand.get();
  }
  return nullptr;
}

GHand* Master::hand(uint16_t slave_index) const {
  if (slave_index == 0) return nullptr;
  for (const auto& hand : hands_) {
    if (hand->GetSlaveIndex() == slave_index) return hand.get();
  }
  return nullptr;
}

}  // namespace ghand
