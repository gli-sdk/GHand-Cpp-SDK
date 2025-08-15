#ifndef XIAOYAO_DEXHAND_H_
#define XIAOYAO_DEXHAND_H_
#include <string>
#include <vector>

#include "xiaoyao/finger.h"

enum HandType { LEFT, RIGHT, NUM_HANDS, NONE };

enum JointId {
    THUMB_DIP,
    THUMB_PIP,
    THUMB_MCP,
    THUMB_SWING,
    THUMB_ROTATION,
    FF_DIP,
    FF_PIP,
    FF_MCP,
    FF_SWING,
    MF_DIP,
    MF_PIP,
    MF_MCP,
    RF_DIP,
    RF_PIP,
    RF_MCP,
    LF_DIP,
    LF_PIP,
    LF_MCP,
    NUM_JOINTS
};

struct MotionParam {
    float angle;
    float velocity;
    float torque;
};

struct Joint {
    JointId id;
    MotionParam target;
    MotionParam state;
};

enum CommType { COMM_ETHERCAT, COMM_CANFD, COMM_RS485 };

class DexHand {
   public:
    DexHand();
    ~DexHand();

    int Open(CommType comm_type, std::string device_name = "auto");
    int Close();
    int MoveJoints(const std::vector<Joint>& joints);
    int GetJoints(std::vector<Joint>* joints);
    std::string GetFirmwareVersion();
    HandType GetHandType();
    // 属性访问(直接返回)
    Thumb thumb() { return thumb_; }
    IndexFinger index_finger() { return index_finger_; }
    MiddleFinger middle_finger() { return middle_finger_; }

    RingFinger ring_finger() { return ring_finger_; };
    LittleFinger little_finger() { return little_finger_; };

   private:
    HandType hand_type_ = HandType::NONE;
    Thumb thumb_;
    IndexFinger index_finger_;
    MiddleFinger middle_finger_;
    RingFinger ring_finger_;
    LittleFinger little_finger_;
    std::string firmware_version_;
};

#endif