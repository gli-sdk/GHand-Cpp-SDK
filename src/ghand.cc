// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include <string> 
#include <memory>
#include <map>
#include "ghand/ghand.h"

#include "internal/ghand_impl.h"

namespace ghand {

std::unique_ptr<GHand> GHand::Create(ProductType pt, CommType ct) {
  auto hand = std::unique_ptr<GHand>(new GHand(pt, ct));
  if (!hand->impl_->IsValid()) {
    return nullptr;
  }
  return hand;
}

GHand::GHand(ProductType pt, CommType ct)
    : impl_(new internal::GHandImpl(pt, ct)) {}

GHand::~GHand() {}

bool GHand::AutoConnect() { return impl_->AutoConnect(); }

bool GHand::Connect(const std::string& device_name, uint8_t slave_id) {
  return impl_->Connect(device_name, slave_id);
}

bool GHand::Connect(const std::string& device_name, uint8_t slave_id,
                    RS485BaudRate baud_rate) {
  return impl_->Connect(device_name, slave_id, baud_rate);
}

bool GHand::Connect(const std::string& device_name, uint8_t slave_id,
                    CANFDBitTiming timing) {
  return impl_->Connect(device_name, slave_id, timing);
}

bool GHand::Disconnect() { return impl_->Disconnect(); }

bool GHand::IsConnected() const { return impl_->IsConnected(); }

bool GHand::SetSlaveId(uint8_t slave_id) {
  return impl_->SetSlaveId(slave_id);
}

bool GHand::SetBaudrateConfig(RS485BaudRate baud_rate) {
  return impl_->SetBaudrateConfig(baud_rate);
}

bool GHand::SetBaudrateConfig(CANFDBitTiming timing) {
  return impl_->SetBaudrateConfig(timing);
}

OperationResult GHand::GetLastResult() const {
  return impl_->GetLastResult();
}

SdkError GHand::GetLastError() const {
  return impl_->GetLastResult().code;
}

std::string GHand::GetLastErrorMessage() const {
  return impl_->GetLastResult().message;
}

Diagnostics GHand::GetDiagnostics() const {
  return impl_->GetDiagnostics();
}

std::map<std::string, std::string> GHand::SearchAdapters() {
  return impl_->SearchAdapters();
}

HandType GHand::GetHandType() { return impl_->GetHandType(); }

DeviceInfo GHand::GetDeviceInfo() { return impl_->GetDeviceInfo(); }

void GHand::SetControlMode(ControlMode mode) { impl_->SetControlMode(mode); }

bool GHand::MoveJoints(const std::vector<JointCommand>& joints) {
  return impl_->MoveJoints(joints);
}

void GHand::Stop() { impl_->Stop(); }

bool GHand::ClearFault() { return impl_->ClearFault(); }

bool GHand::InitJoint() { return impl_->InitJoint(); }

bool GHand::OpenTactile() { return impl_->OpenTactile(); }

bool GHand::CloseTactile() { return impl_->CloseTactile(); }

bool GHand::ZeroTactile() { return impl_->ZeroTactile(); }

void GHand::SetJointsCallback(JointsCallback cb) {
  impl_->SetJointsCallback(cb);
}

void GHand::SetHandStateCallback(HandStateCallback cb) {
  impl_->SetHandStateCallback(cb);
}

void GHand::SetTactileDataCallback(TactileDataCallback cb) {
  impl_->SetTactileDataCallback(cb);
}

HandState GHand::GetHandData() { return impl_->GetHandData(); }

std::vector<Joint> GHand::GetJointsData() { return impl_->GetJointsData(); }

TactileData GHand::GetTactileData() { return impl_->GetTactileData(); }

SelfTestErrorInfo GHand::GetSelfTestErrorInfo() {
  return impl_->GetSelfTestErrorInfo();
}

FirmwareUpdateError GHand::BootUpdate(
    const std::string& filename,
    std::function<void(int)> progress_callback) {
  return impl_->BootUpdate(filename, progress_callback);
}

}  // namespace ghand
