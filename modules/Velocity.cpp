
#include "Velocity.hpp"

#include <iostream>
#include <jni.h>
#include "../classes/Entity.hpp"
#include "../jni/MinecraftMappings.hpp"

Velocity::Velocity(JNIEnv *p_env) : env(p_env) {
    setDeltaMovementID = env->GetMethodID(
        LocalPlayer::getLocalPlayerClass(),
        mc_mappings::local_player::SetDeltaMovement.name,
        mc_mappings::local_player::SetDeltaMovement.signature);
    if (setDeltaMovementID == nullptr) {
        std::cout << "[ERROR] setDeltaMovement method ID not found." << std::endl;
    }
    hurtTimeFieldID = env->GetFieldID(
        LocalPlayer::getLocalPlayerClass(),
        mc_mappings::local_player::HurtTime.name,
        mc_mappings::local_player::HurtTime.signature);
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
