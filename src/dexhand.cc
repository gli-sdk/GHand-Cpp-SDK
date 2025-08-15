#include "xiaoyao/dexhand.h"

DexHand::DexHand() {}

DexHand::~DexHand() {}

int DexHand::Open(CommType comm_type, std::string device_name) {
    switch (comm_type) {
        case COMM_ETHERCAT:
            /* code */
            break;
        case COMM_CANFD:
            /* code */
            break;
        case COMM_RS485:
            /* code */
            break;
        default:
            break;
    }

    return 0;
}

int DexHand::Close() { return 0; }

int DexHand::MoveJoints(const std::vector<Joint>& joints) {
    for (auto joint : joints) {
        if (joint.id >= NUM_JOINTS) {
            return -1;
        }
        // TODO:发送命令
    }
    return 0;
}
int DexHand::GetJoints(std::vector<Joint>* joints) {
    // TODO:返回每个joint的状态

    return 0;
}

std::string DexHand::GetFirmwareVersion() {
    if (firmware_version_.empty()) {
        // TODO:获取固件版本
    }
    return firmware_version_;
}

HandType DexHand::GetHandType() {
    if (hand_type_ == HandType::NONE) {
        // TODO:获取手类型
    }
    return hand_type_;
}
