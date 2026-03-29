#include "FastBreak.hpp"

#include <iostream>

#include "../classes/minecraft.hpp"
#include "../jni/JniEnvironment.hpp"
#include "../jni/MinecraftMappings.hpp"

namespace {
bool ClearFastBreakException(JNIEnv *env, const char *context) {
    if (env == nullptr || !env->ExceptionCheck()) {
        return false;
    }

    std::cout << "[FastBreak] JNI call failed at " << context << std::endl;
    env->ExceptionClear();
    return true;
}
} // namespace

FastBreak::FastBreak(Minecraft *p_mc) : mc(p_mc) {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat");
    if (env == nullptr || mc == nullptr || mc->getMinecraftClass() == nullptr) {
        return;
    }

    gameModeFieldID = env->GetFieldID(
        mc->getMinecraftClass(),
        mc_mappings::minecraft::GameMode.name,
        mc_mappings::minecraft::GameMode.signature);
    if (gameModeFieldID == nullptr || ClearFastBreakException(env, "Minecraft.gameMode")) {
        std::cout << "[ERROR] GameMode field ID not found." << std::endl;
        return;
    }

    jclass localGameModeClass = env->FindClass(mc_mappings::classes::GameMode);
    if (localGameModeClass == nullptr) {
        ClearFastBreakException(env, "FindClass(GameMode)");
        std::cout << "[ERROR] GameMode class not found." << std::endl;
        return;
    }

    gameModeClass = reinterpret_cast<jclass>(env->NewGlobalRef(localGameModeClass));
    env->DeleteLocalRef(localGameModeClass);
    if (gameModeClass == nullptr) {
        return;
    }

    fid_destroydelay = env->GetFieldID(
        gameModeClass,
        mc_mappings::game_mode::DestroyDelay.name,
        mc_mappings::game_mode::DestroyDelay.signature);
    fid_destroyprogress = env->GetFieldID(
        gameModeClass,
        mc_mappings::game_mode::DestroyProgress.name,
        mc_mappings::game_mode::DestroyProgress.signature);
    if (fid_destroydelay == nullptr || fid_destroyprogress == nullptr ||
        ClearFastBreakException(env, "GameMode fields")) {
        std::cout << "[ERROR] GameMode field lookup failed." << std::endl;
    }
}

FastBreak::~FastBreak() {
    JNIEnv *currentEnv = JniEnvironment::GetCurrentEnv();
    if (currentEnv == nullptr) {
        return;
    }

    if (gameModeClass != nullptr) {
        currentEnv->DeleteGlobalRef(gameModeClass);
        gameModeClass = nullptr;
    }
}

void FastBreak::break_fast() const {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat Render");
    if (env == nullptr || gameModeFieldID == nullptr || fid_destroydelay == nullptr || Minecraft::getMcInstance() == nullptr) {
        return;
    }

    jobject gameModeObj = env->GetObjectField(Minecraft::getMcInstance(), gameModeFieldID);
    if (gameModeObj == nullptr || ClearFastBreakException(env, "Minecraft.gameMode read")) {
        if (gameModeObj != nullptr) {
            env->DeleteLocalRef(gameModeObj);
        }
        return;
    }

    env->SetIntField(gameModeObj, fid_destroydelay, -5);
    ClearFastBreakException(env, "GameMode.destroyDelay write");
    env->DeleteLocalRef(gameModeObj);
}

void FastBreak::insta_break() const {
}
