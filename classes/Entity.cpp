#include "Entity.hpp"

#include <iostream>

#include "minecraft.hpp"
#include "../jni/MinecraftMappings.hpp"

Entity::Entity(JNIEnv *p_env, Minecraft *pmc) : env(p_env), mc(pmc) {
    jclass localclass = env->FindClass(mc_mappings::classes::Entity);
    (void) localclass;
}

LocalPlayer::LocalPlayer(JNIEnv *p_env, Minecraft *pmc) : env(p_env), mc(pmc) {
    jclass localclass = env->FindClass(mc_mappings::classes::LocalPlayer);
    if (localclass == nullptr) {
        std::cout << "[ERROR] LocalPlayer class not found." << std::endl;
    }
    playerClass = (jclass) env->NewGlobalRef(localclass);
    env->DeleteLocalRef(localclass);
    jfieldID player_fid = env->GetFieldID(
        mc->getMinecraftClass(),
        mc_mappings::minecraft::LocalPlayer.name,
        mc_mappings::minecraft::LocalPlayer.signature);
    if (player_fid == nullptr) {
        std::cout << "[ERROR] LocalPlayer field ID not found." << std::endl;
    }
    jobject localInstance = env->GetObjectField(mc->getMcInstance(), player_fid);
    if (localInstance == nullptr) {
        std::cout << "[ERROR] LocalPlayer object not found." << std::endl;
    }
    playerObject = env->NewGlobalRef(localInstance);
    env->DeleteLocalRef(localInstance);
    SetSprinting = env->GetMethodID(
        playerClass,
        mc_mappings::local_player::SetSprinting.name,
        mc_mappings::local_player::SetSprinting.signature);
    if (SetSprinting == nullptr) {
        std::cout << "[ERROR] SetSprinting method not found." << std::endl;
    }
}

void LocalPlayer::Sprint() {
    env->CallVoidMethod(playerObject, SetSprinting, JNI_TRUE);
}

jobject LocalPlayer::getLocalPlayerObject() {
    return playerObject;
}
jclass LocalPlayer::getLocalPlayerClass() {
    return playerClass;
}
