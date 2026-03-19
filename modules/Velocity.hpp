#pragma once

#include <jni.h>

class Velocity {
    jmethodID setDeltaMovementID{};
    jfieldID hurtTimeFieldID{};
    JNIEnv *env{};

public:
    Velocity();
    void anti_knockback() const;
};