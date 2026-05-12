#include "ghand/dexhand.h"
#include "internal/dexhand.h"

namespace xiaoyao {

// 工厂函数
std::unique_ptr<DexHand> DexHand::Create(ProductType pt, CommType ct) {
    auto hand = std::unique_ptr<DexHand>(new DexHand(pt, ct));
    if (!hand->impl_->IsValid()) {
        return nullptr;
    }
    return hand;
}

// 构造/析构
DexHand::DexHand(ProductType pt, CommType ct)
    : impl_(new internal::DexHand(pt, ct)) {}
DexHand::~DexHand() {}

// 转发所有调用
bool DexHand::AutoConnect() {
    return impl_->AutoConnect();
}

bool DexHand::Connect(const std::string& device_name) {
    return impl_->Connect(device_name);
}

bool DexHand::Disconnect() {
    return impl_->Disconnect();
}

bool DexHand::IsConnected() const {
    return impl_->IsConnected();
}

std::map<std::string, std::string> DexHand::SearchAdapters() {
    return impl_->SearchAdapters();
}

HandType DexHand::GetHandType() {
    return impl_->GetHandType();
}

DeviceInfo DexHand::GetDeviceInfo() {
    return impl_->GetDeviceInfo();
}

void DexHand::SetControlMode(ControlMode mode) {
    impl_->SetControlMode(mode);
}

bool DexHand::MoveJoints(const std::vector<JointCommand>& joints) {
    return impl_->MoveJoints(joints);
}

void DexHand::Stop() {
    impl_->Stop();
}

bool DexHand::ClearFault() {
    return impl_->ClearFault();
}

bool DexHand::InitJoint() {
    return impl_->InitJoint();
}

bool DexHand::OpenTactile() {
    return impl_->OpenTactile();
}

bool DexHand::CloseTactile() {
    return impl_->CloseTactile();
}

bool DexHand::ZeroTactile() {
    return impl_->ZeroTactile();
}

void DexHand::SetJointsCallback(JointsCallback cb) {
    impl_->SetJointsCallback(cb);
}

void DexHand::SetHandStateCallback(HandStateCallback cb) {
    impl_->SetHandStateCallback(cb);
}

void DexHand::SetTactileDataCallback(TactileDataCallback cb) {
    impl_->SetTactileDataCallback(cb);
}

int DexHand::BootUpdate(const std::string& ifname,
                   uint16_t slave,
                   const std::string& filename,
                   std::function<void(int)> progressCallback) {
    return impl_->BootUpdate(ifname, slave, filename, progressCallback);
}
}  // namespace xiaoyao
