#include "Velocity.hpp"

#include <iostream>

#include "../classes/Entity.hpp"
#include "../jni/JniEnvironment.hpp"
#include "../jni/MinecraftMappings.hpp"

namespace {
bool ClearVelocityException(JNIEnv *env, const char *context) {
    if (env == nullptr || !env->ExceptionCheck()) {
        return false;
    }

    std::cout << "[Velocity] JNI call failed at " << context << std::endl;
    env->ExceptionClear();
    return true;
}
} // namespace

Velocity::Velocity() {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat");
    if (env == nullptr || LocalPlayer::getLocalPlayerClass() == nullptr) {
        return;
    }

    setDeltaMovementID = env->GetMethodID(
        LocalPlayer::getLocalPlayerClass(),
        mc_mappings::local_player::SetDeltaMovement.name,
        mc_mappings::local_player::SetDeltaMovement.signature);
    if (setDeltaMovementID == nullptr || ClearVelocityException(env, "LocalPlayer.setDeltaMovement")) {
        std::cout << "[ERROR] setDeltaMovement method ID not found." << std::endl;
    }

    hurtTimeFieldID = env->GetFieldID(
        LocalPlayer::getLocalPlayerClass(),
        mc_mappings::local_player::HurtTime.name,
        mc_mappings::local_player::HurtTime.signature);
    if (hurtTimeFieldID == nullptr || ClearVelocityException(env, "LocalPlayer.hurtTime")) {
        std::cout << "[ERROR] hurtTime field ID not found." << std::endl;
    }
}

void Velocity::anti_knockback() const {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat Render");
    if (env == nullptr || setDeltaMovementID == nullptr || hurtTimeFieldID == nullptr) {
        return;
    }

    jobject localPlayer = LocalPlayer::getLocalPlayerObject();
    if (localPlayer == nullptr) {
        return;
    }

    const jint isHurt = env->GetIntField(localPlayer, hurtTimeFieldID);
    if (ClearVelocityException(env, "LocalPlayer.hurtTime read")) {
        env->DeleteLocalRef(localPlayer);
        return;
    }

    if (isHurt >= 8) {
        env->CallVoidMethod(localPlayer, setDeltaMovementID, 0.0, 0.0, 0.0);
        ClearVelocityException(env, "LocalPlayer.setDeltaMovement");
    }
    env->DeleteLocalRef(localPlayer);
}
