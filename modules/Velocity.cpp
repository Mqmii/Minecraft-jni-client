
#include "Velocity.hpp"

#include <iostream>
#include <jni.h>
#include "../classes/Entity.hpp"

Velocity::Velocity(JNIEnv *p_env) : env(p_env) {
    setDeltaMovementID = env->GetMethodID(LocalPlayer::getLocalPlayerClass(), "m", "(DDD)V");
    if (setDeltaMovementID == nullptr) {
        std::cout << "[ERROR] setDeltaMovement method ID not found." << std::endl;
    }
    hurtTimeFieldID = env->GetFieldID(LocalPlayer::getLocalPlayerClass(),"bu","I");
    if (hurtTimeFieldID == nullptr) {
        std::cout << "[ERROR] hurtTime field ID not found." << std::endl;
    }
}

void Velocity::anti_knockback() const {
    jint ishurt = env->GetIntField(LocalPlayer::getLocalPlayerObject(),hurtTimeFieldID);
    if (ishurt >= 8) {
        env->CallVoidMethod(LocalPlayer::getLocalPlayerObject(), setDeltaMovementID, 0.0, 0.0, 0.0);
    }
}
