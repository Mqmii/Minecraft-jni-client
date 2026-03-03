#include "Entity.hpp"

#include <iostream>

#include "minecraft.hpp"

Entity::Entity(JNIEnv *p_env, Minecraft *pmc) : env(p_env), mc(pmc) {
    jclass localclass = env->FindClass("cgk");
    (void) localclass;
}

LocalPlayer::LocalPlayer(JNIEnv *p_env, Minecraft *pmc) : env(p_env), mc(pmc) {
    jclass localclass = env->FindClass("hnh");
    if (localclass == nullptr) {
        std::cout << "[ERROR] LocalPlayer class not found." << std::endl;
    }
    playerClass = (jclass) env->NewGlobalRef(localclass);
    env->DeleteLocalRef(localclass);
    jfieldID player_fid = env->GetFieldID(mc->getMinecraftClass(), "s", "Lhnh;");
    if (player_fid == nullptr) {
        std::cout << "[ERROR] LocalPlayer field ID not found." << std::endl;
    }
    jobject localInstance = env->GetObjectField(mc->getMcInstance(), player_fid);
    if (localInstance == nullptr) {
        std::cout << "[ERROR] LocalPlayer object not found." << std::endl;
    }
    playerObject = env->NewGlobalRef(localInstance);
    env->DeleteLocalRef(localInstance);
    SetSprinting = env->GetMethodID(playerClass, "i", "(Z)V");
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
