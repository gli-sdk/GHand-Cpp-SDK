#include "ghand/ghand.h"

#include "internal/dexhand.h"

namespace ghand {

// Factory function
std::unique_ptr<DexHand> DexHand::Create(ProductType pt, CommType ct) {
  auto hand = std::unique_ptr<DexHand>(new DexHand(pt, ct));
  if (!hand->impl_->IsValid()) {
    return nullptr;
  }
  return hand;
}

// Constructor / Destructor
DexHand::DexHand(ProductType pt, CommType ct)
    : impl_(new internal::DexHand(pt, ct)) {}
DexHand::~DexHand() {}

// Forward all calls
bool DexHand::AutoConnect() { return impl_->AutoConnect(); }

bool DexHand::Connect(const std::string& device_name) {
  return impl_->Connect(device_name);
}

bool DexHand::Disconnect() { return impl_->Disconnect(); }

bool DexHand::IsConnected() const { return impl_->IsConnected(); }

std::map<std::string, std::string> DexHand::SearchAdapters() {
  return impl_->SearchAdapters();
}

HandType DexHand::GetHandType() { return impl_->GetHandType(); }

DeviceInfo DexHand::GetDeviceInfo() { return impl_->GetDeviceInfo(); }

void DexHand::SetControlMode(ControlMode mode) { impl_->SetControlMode(mode); }

bool DexHand::MoveJoints(const std::vector<JointCommand>& joints) {
  return impl_->MoveJoints(joints);
}

void DexHand::Stop() { impl_->Stop(); }

bool DexHand::ClearFault() { return impl_->ClearFault(); }

bool DexHand::InitJoint() { return impl_->InitJoint(); }

bool DexHand::OpenTactile() { return impl_->OpenTactile(); }

bool DexHand::CloseTactile() { return impl_->CloseTactile(); }

bool DexHand::ZeroTactile() { return impl_->ZeroTactile(); }

void DexHand::SetJointsCallback(JointsCallback cb) {
  impl_->SetJointsCallback(cb);
}

void DexHand::SetHandStateCallback(HandStateCallback cb) {
  impl_->SetHandStateCallback(cb);
}

void DexHand::SetTactileDataCallback(TactileDataCallback cb) {
  impl_->SetTactileDataCallback(cb);
}

HandState DexHand::GetHandData() { return impl_->GetHandData(); }

std::vector<Joint> DexHand::GetJointsData() { return impl_->GetJointsData(); }

TactileData DexHand::GetTactileData() { return impl_->GetTactileData(); }

int DexHand::BootUpdate(const std::string& ifname, uint16_t slave,
                        const std::string& filename,
                        std::function<void(int)> progress_callback) {
  return impl_->BootUpdate(ifname, slave, filename, progress_callback);
}
}  // namespace ghand
