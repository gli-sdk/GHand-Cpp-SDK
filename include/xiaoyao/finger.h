#ifndef XIAOYAO_FINGER_H_
#define XIAOYAO_FINGER_H_
#include <cstdint>
#include <vector>

enum FingerType { THUMB, FF, MF, RF, LF, NUM_FINGERS };
class Force {
   public:
    float x;
    float y;
    float z;
    Force() : x(0.0), y(0.0), z(0.0) {}
    Force(int16_t x_val, int16_t y_val, uint16_t z_val) : x(x_val), y(y_val), z(z_val) {}
};

class Finger {
   public:
    Finger(FingerType type) { type_ = type; }
    virtual ~Finger() {}

    virtual int GetResultantForceOffset() const;
    virtual int GetSampleForcesOffset() const;
    virtual int GetResultantForceSize() const;
    virtual int GetSampleForcesSize() const;
    virtual Force GetResultantForce(const uint8_t* data, int data_size) const;
    virtual std::vector<Force> GetSampleForces(const uint8_t* data, int data_size) const;

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