#include "Entity.hpp"

#include <iostream>
#include <windows.h>
#include "minecraft.hpp"
#include "../jni/JniEnvironment.hpp"
#include "../jni/MinecraftMappings.hpp"

Entity::Entity(Minecraft *pmc) : env(JniEnvironment::GetCurrentEnv()), mc(pmc) {
    jclass localclass = env->FindClass(mc_mappings::classes::Entity);
    (void) localclass;
    clsEntity = reinterpret_cast<jclass>(env->NewGlobalRef(localclass));
}

LocalPlayer::LocalPlayer(Minecraft *pmc) : env(JniEnvironment::GetCurrentEnv()), mc(pmc) {
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
    playerFieldID = player_fid;
    SetSprinting = env->GetMethodID(
        playerClass,
        mc_mappings::local_player::SetSprinting.name,
        mc_mappings::local_player::SetSprinting.signature);
    if (SetSprinting == nullptr) {
        std::cout << "[ERROR] SetSprinting method not found." << std::endl;
    }
}

void LocalPlayer::Sprint() {
    jobject playerObject = getLocalPlayerObject();
    if (playerObject == nullptr) {
        return;
    }

    if ((GetAsyncKeyState('W') & 0x8000) != 0) {
        env->CallVoidMethod(playerObject, SetSprinting, JNI_TRUE);
    }
    env->DeleteLocalRef(playerObject);
}

jobject LocalPlayer::getLocalPlayerObject() {
    JNIEnv *currentEnv = JniEnvironment::GetCurrentEnv();
    if (currentEnv == nullptr || playerFieldID == nullptr || Minecraft::getMcInstance() == nullptr) {
        return nullptr;
    }

    return currentEnv->GetObjectField(Minecraft::getMcInstance(), playerFieldID);
}

jclass LocalPlayer::getLocalPlayerClass() {
    return playerClass;
}
