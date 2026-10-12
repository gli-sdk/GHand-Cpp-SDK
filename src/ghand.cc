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
                                     HandType hand_type,
                                     const DeviceInfo& device_info) {
    return std::unique_ptr<GHand>(
        new GHand(impl, slave_index, hand_type, device_info));
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
        impl, hand.slave_index, hand.hand_type, hand.device_info));
    hands.push_back(storage->back().get());
  }
  return hands;
}

}  // namespace

// ---------------------------------------------------------------------------
// GHand
// ---------------------------------------------------------------------------

GHand::GHand(internal::GHandImpl* impl, uint16_t slave_index,
             HandType hand_type, const DeviceInfo& device_info)
    : impl_(impl),
      slave_index_(slave_index),
      hand_type_(hand_type),
      device_info_(device_info) {}

GHand::~GHand() {}

ResultOf<DeviceInfo> GHand::QueryDeviceInfo() {
  return impl_->GetDeviceInfo(slave_index_);
}

Result GHand::MoveJoints(const std::vector<JointCommand>& joints,
                         ControlMode mode) {
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

ResultOf<HandState> GHand::GetHandData() {
  return impl_->GetHandData(slave_index_);
}

ResultOf<std::vector<Joint>> GHand::GetJointsData() {
  return impl_->GetJointsData(slave_index_);
}

ResultOf<TactileData> GHand::GetTactileData() {
  return impl_->GetTactileData(slave_index_);
}

ResultOf<SelfTestErrorInfo> GHand::GetSelfTestErrorInfo() {
  return impl_->GetSelfTestErrorInfo(slave_index_);
}

ResultOf<uint8_t> GHand::GetSelfTestStatus() {
  return impl_->GetSelfTestStatus(slave_index_);
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

Master::~Master() {}

ResultOf<std::vector<GHand*>> Master::Connect(const std::string& device_name,
                                              uint8_t slave_id) {
  return AdoptHands(impl_.get(), &hands_, impl_->Connect(device_name, slave_id));
}

ResultOf<std::vector<GHand*>> Master::Connect(const std::string& device_name,
                                              uint8_t slave_id,
                                              RS485BaudRate baud_rate) {
  return AdoptHands(impl_.get(), &hands_,
                    impl_->Connect(device_name, slave_id, baud_rate));
}

ResultOf<std::vector<GHand*>> Master::Connect(const std::string& device_name,
                                              uint8_t slave_id,
                                              CANFDBitTiming timing) {
  return AdoptHands(impl_.get(), &hands_,
                    impl_->Connect(device_name, slave_id, timing));
}

Result Master::PrepareCANFDBaudrateListener(const std::string& device_name,
                                             CANFDBitTiming timing) {
  return impl_->PrepareCANFDBaudrateListener(device_name, timing);
}

DeviceInfo Master::GetDeviceInfo() const { return impl_->GetLastDeviceInfo(); }

Result Master::Disconnect() { return impl_->Disconnect(); }

bool Master::IsConnected() const { return impl_->IsConnected(); }

std::map<std::string, std::string> Master::SearchAdapters() {
  return impl_->SearchAdapters();
}

Diagnostics Master::GetDiagnostics() const { return impl_->GetDiagnostics(); }

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
    if (hand->hand_type() == type) return hand.get();
  }
  return nullptr;
}

GHand* Master::hand(uint16_t slave_index) const {
  if (slave_index == 0) return nullptr;
  for (const auto& hand : hands_) {
    if (hand->slave_index() == slave_index) return hand.get();
  }
  return nullptr;
}

Result Master::MoveJoints(const std::vector<JointCommand>& joints,
                          ControlMode mode) {
  // slave_index 0 = every hand on the bus.
  return impl_->MoveJoints(0, joints, mode);
}

Result Master::Stop() { return impl_->Stop(0); }

Result Master::ClearFault() { return impl_->ClearFault(0); }

FirmwareUpdateError Master::BootUpdate(
    const std::string& filename,
    std::function<void(int)> progress_callback) {
  return impl_->BootUpdate(filename, progress_callback);
}

}  // namespace ghand
