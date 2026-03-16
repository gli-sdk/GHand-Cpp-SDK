#include "xiaoyao/xiaoyao.h"
#include "internal/dexhand.h"

namespace xiaoyao {

// Pimpl 实现
class DexHand::Impl {
public:
    internal::DexHand hand_;
};

// 构造/析构
DexHand::DexHand() : impl_(new Impl()) {}
DexHand::~DexHand() {}

// 转发所有调用
bool DexHand::AutoConnect(CommType comm_type) {
    return impl_->hand_.AutoConnect(comm_type);
}

bool DexHand::Connect(CommType comm_type, const std::string& device_name) {
    return impl_->hand_.Connect(comm_type, device_name);
}

bool DexHand::Disconnect() {
    return impl_->hand_.Disconnect();
}

bool DexHand::IsConnected() const {
    return impl_->hand_.IsConnected();
}

std::map<std::string, std::string> DexHand::SearchAdapters() const {
    return impl_->hand_.SearchAdapters();
}

HandType DexHand::GetHandType() const {
    return impl_->hand_.GetHandType();
}

DeviceInfo DexHand::GetDeviceInfo() const {
    return impl_->hand_.GetDeviceInfo();
}

void DexHand::SetControlMode(ControlMode mode) {
    impl_->hand_.SetControlMode(mode);
}

bool DexHand::MoveJoints(const std::vector<JointCommand>& joints) {
    return impl_->hand_.MoveJoints(joints);
}

void DexHand::Stop() {
    impl_->hand_.Stop();
}

bool DexHand::ClearFault() {
    return impl_->hand_.ClearFault();
}

bool DexHand::InitJoint() {
    return impl_->hand_.InitJoint();
}

bool DexHand::OpenTactile() {
    return impl_->hand_.OpenTactile();
}

bool DexHand::CloseTactile() {
    return impl_->hand_.CloseTactile();
}

bool DexHand::ZeroTactile() {
    return impl_->hand_.ZeroTactile();
}

void DexHand::SetJointsCallback(JointsCallback cb) {
    impl_->hand_.SetJointsCallback(cb);
}

void DexHand::SetHandStateCallback(HandStateCallback cb) {
    impl_->hand_.SetHandStateCallback(cb);
}

void DexHand::SetTactileDataCallback(TactileDataCallback cb) {
    impl_->hand_.SetTactileDataCallback(cb);
}

int DexHand::BootUpdate(const std::string& ifname,
                   uint16_t slave,
                   const std::string& filename,
                   std::function<void(int)> progressCallback) {
    return impl_->hand_.BootUpdate(ifname, slave, filename, progressCallback);
}
}  // namespace xiaoyao
