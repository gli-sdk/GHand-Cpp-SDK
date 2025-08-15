#ifndef XIAOYAO_FINGER_H_
#define XIAOYAO_FINGER_H_
enum FingerType { THUMB, FF, MF, RF, LF, NUM_FINGERS };

class Finger {
   public:
    Finger(FingerType type) { type_ = type; }
    ~Finger() {}

   private:
    FingerType type_;
};

class Thumb : Finger {
   public:
    Thumb() : Finger(THUMB) {}
};

class IndexFinger : Finger {
   public:
    IndexFinger() : Finger(FF) {}
};

class MiddleFinger : Finger {
   public:
    MiddleFinger() : Finger(MF) {}
};

class RingFinger : Finger {
   public:
    RingFinger() : Finger(RF) {}
};

class LittleFinger : Finger {
   public:
    LittleFinger() : Finger(LF) {}
};
#endif  // XIAOYAO_FINGER_H_