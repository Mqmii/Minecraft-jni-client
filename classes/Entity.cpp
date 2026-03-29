#include "Entity.hpp"

#include <iostream>
#include <windows.h>

#include "minecraft.hpp"
#include "../jni/JniEnvironment.hpp"
#include "../jni/MinecraftMappings.hpp"

namespace {
bool ClearEntityException(JNIEnv *env, const char *context) {
    if (env == nullptr || !env->ExceptionCheck()) {
        return false;
    }

    std::cout << "[Entity] JNI call failed at " << context << std::endl;
    env->ExceptionClear();
    return true;
}
} // namespace

Entity::Entity(Minecraft *pmc) : mc(pmc) {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat");
    if (env == nullptr) {
        return;
    }

    jclass localClass = env->FindClass(mc_mappings::classes::Entity);
    if (localClass == nullptr) {
        ClearEntityException(env, "FindClass(Entity)");
        return;
    }

    clsEntity = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
    env->DeleteLocalRef(localClass);
}

Entity::~Entity() {
    JNIEnv *currentEnv = JniEnvironment::GetCurrentEnv();
    if (currentEnv == nullptr) {
        return;
    }

    if (clsEntity != nullptr) {
        currentEnv->DeleteGlobalRef(clsEntity);
        clsEntity = nullptr;
    }
}

LocalPlayer::LocalPlayer(Minecraft *pmc) : mc(pmc) {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat");
    if (env == nullptr) {
        return;
    }

    jclass localClass = env->FindClass(mc_mappings::classes::LocalPlayer);
    if (localClass == nullptr) {
        ClearEntityException(env, "FindClass(LocalPlayer)");
        std::cout << "[ERROR] LocalPlayer class not found." << std::endl;
        return;
    }

    playerClass = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
    env->DeleteLocalRef(localClass);
    if (playerClass == nullptr) {
        return;
    }

    const jfieldID playerFid = env->GetFieldID(
        mc->getMinecraftClass(),
        mc_mappings::minecraft::LocalPlayer.name,
        mc_mappings::minecraft::LocalPlayer.signature);
    if (playerFid == nullptr || ClearEntityException(env, "Minecraft.localPlayer")) {
        std::cout << "[ERROR] LocalPlayer field ID not found." << std::endl;
        return;
    }
    playerFieldID = playerFid;

    SetSprinting = env->GetMethodID(
        playerClass,
        mc_mappings::local_player::SetSprinting.name,
        mc_mappings::local_player::SetSprinting.signature);
    if (SetSprinting == nullptr || ClearEntityException(env, "LocalPlayer.setSprinting")) {
        std::cout << "[ERROR] SetSprinting method not found." << std::endl;
    }
}

LocalPlayer::~LocalPlayer() {
    JNIEnv *currentEnv = JniEnvironment::GetCurrentEnv();
    if (currentEnv == nullptr) {
        return;
    }

    if (playerClass != nullptr) {
        currentEnv->DeleteGlobalRef(playerClass);
        playerClass = nullptr;
    }
    playerFieldID = nullptr;
    SetSprinting = nullptr;
}

void LocalPlayer::Sprint() {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat Render");
    if (env == nullptr || SetSprinting == nullptr) {
        return;
    }

    jobject playerObject = getLocalPlayerObject();
    if (playerObject == nullptr) {
        return;
    }

    if ((GetAsyncKeyState('W') & 0x8000) != 0) {
        env->CallVoidMethod(playerObject, SetSprinting, JNI_TRUE);
        ClearEntityException(env, "LocalPlayer.setSprinting");
    }
    env->DeleteLocalRef(playerObject);
}

jobject LocalPlayer::getLocalPlayerObject() {
    JNIEnv *currentEnv = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat Render");
    if (currentEnv == nullptr || playerFieldID == nullptr || Minecraft::getMcInstance() == nullptr) {
        return nullptr;
    }

    jobject playerObject = currentEnv->GetObjectField(Minecraft::getMcInstance(), playerFieldID);
    if (ClearEntityException(currentEnv, "Minecraft.localPlayer read")) {
        if (playerObject != nullptr) {
            currentEnv->DeleteLocalRef(playerObject);
        }
        return nullptr;
    }
    return playerObject;
}

jclass LocalPlayer::getLocalPlayerClass() {
    return playerClass;
}
