#pragma once

#include <jni.h>

class Velocity {
    jmethodID setDeltaMovementID{};
    jfieldID hurtTimeFieldID{};

public:
    Velocity();
    void anti_knockback() const;
};
