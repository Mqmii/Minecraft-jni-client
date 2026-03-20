#include "Velocity.hpp"

#include <iostream>

#include "../classes/Entity.hpp"
#include "../jni/JniEnvironment.hpp"
#include "../jni/MinecraftMappings.hpp"

Velocity::Velocity() : env(JniEnvironment::GetCurrentEnv()) {
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
//todo: improve it with manipulating packets
void Velocity::anti_knockback() const {
    jobject localPlayer = LocalPlayer::getLocalPlayerObject();
    if (localPlayer == nullptr) {
        return;
    }

    jint ishurt = env->GetIntField(localPlayer, hurtTimeFieldID);
    if (ishurt >= 8) {
        env->CallVoidMethod(localPlayer, setDeltaMovementID, 0.0, 0.0, 0.0);
    }
    env->DeleteLocalRef(localPlayer);
}
