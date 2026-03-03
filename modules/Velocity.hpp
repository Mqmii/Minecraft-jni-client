
#ifndef MINECRAFTWRAPPERJNI_VELOCITY_HPP
#define MINECRAFTWRAPPERJNI_VELOCITY_HPP
#include <jni.h>

class Velocity {
    jmethodID setDeltaMovementID{};
    jfieldID hurtTimeFieldID{};
    JNIEnv *env;
public:
    Velocity(JNIEnv *p_env);
    void anti_knockback() const;
};

#endif
