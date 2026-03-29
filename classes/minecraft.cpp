#include "minecraft.hpp"

#include <iostream>

#include "../jni/JniEnvironment.hpp"
#include "../jni/MinecraftMappings.hpp"

namespace {
bool ClearMinecraftException(JNIEnv *env, const char *context) {
    if (env == nullptr || !env->ExceptionCheck()) {
        return false;
    }

    std::cout << "[Minecraft] JNI call failed at " << context << std::endl;
    env->ExceptionClear();
    return true;
}
} // namespace

Minecraft::Minecraft() {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat");
    if (env == nullptr) {
        std::cout << "[ERROR] Failed to resolve JNIEnv for Minecraft wrapper." << std::endl;
        return;
    }

    jclass localClass = env->FindClass(mc_mappings::classes::Minecraft);
    if (localClass == nullptr) {
        ClearMinecraftException(env, "FindClass(Minecraft)");
        std::cout << "[ERROR] Minecraft class not found." << std::endl;
        return;
    }

    minecraftClass = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
    env->DeleteLocalRef(localClass);
    if (minecraftClass == nullptr) {
        std::cout << "[ERROR] Failed to create global Minecraft class ref." << std::endl;
        return;
    }

    const jfieldID fidInstance = env->GetStaticFieldID(
        minecraftClass,
        mc_mappings::minecraft::Instance.name,
        mc_mappings::minecraft::Instance.signature);
    if (fidInstance == nullptr || ClearMinecraftException(env, "Minecraft.Instance")) {
        std::cout << "[ERROR] Minecraft instance field not found." << std::endl;
        return;
    }

    jobject localInstance = env->GetStaticObjectField(minecraftClass, fidInstance);
    if (localInstance == nullptr || ClearMinecraftException(env, "Minecraft.Instance read")) {
        if (localInstance != nullptr) {
            env->DeleteLocalRef(localInstance);
        }
        std::cout << "[ERROR] Minecraft instance object not found." << std::endl;
        return;
    }

    mcInstance = env->NewGlobalRef(localInstance);
    env->DeleteLocalRef(localInstance);
    if (mcInstance == nullptr) {
        std::cout << "[ERROR] Failed to create global Minecraft instance ref." << std::endl;
    }
}

Minecraft::~Minecraft() {
    JNIEnv *currentEnv = JniEnvironment::GetCurrentEnv();
    if (currentEnv == nullptr) {
        return;
    }

    if (mcInstance != nullptr) {
        currentEnv->DeleteGlobalRef(mcInstance);
        mcInstance = nullptr;
    }
    if (minecraftClass != nullptr) {
        currentEnv->DeleteGlobalRef(minecraftClass);
        minecraftClass = nullptr;
    }
}

jobject Minecraft::getMcInstance() {
    return mcInstance;
}

jclass Minecraft::getMinecraftClass() {
    return minecraftClass;
}

void Minecraft::fullBright() const {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat");
    if (env == nullptr || minecraftClass == nullptr || mcInstance == nullptr) {
        return;
    }

    const jfieldID fidOptions = env->GetFieldID(
        minecraftClass,
        mc_mappings::minecraft::Options.name,
        mc_mappings::minecraft::Options.signature);
    if (fidOptions == nullptr || ClearMinecraftException(env, "Minecraft.options")) {
        std::cout << "[ERROR] options field ID not found." << std::endl;
        return;
    }

    jobject optionsObj = env->GetObjectField(mcInstance, fidOptions);
    if (optionsObj == nullptr || ClearMinecraftException(env, "Minecraft.options read")) {
        if (optionsObj != nullptr) {
            env->DeleteLocalRef(optionsObj);
        }
        std::cout << "[ERROR] options object not found." << std::endl;
        return;
    }

    jclass clsOptions = env->GetObjectClass(optionsObj);
    if (clsOptions == nullptr) {
        env->DeleteLocalRef(optionsObj);
        return;
    }

    const jfieldID gammaFid = env->GetFieldID(
        clsOptions,
        mc_mappings::options::Gamma.name,
        mc_mappings::options::Gamma.signature);
    if (gammaFid == nullptr || ClearMinecraftException(env, "Options.gamma")) {
        std::cout << "[ERROR] gamma field ID not found." << std::endl;
        env->DeleteLocalRef(clsOptions);
        env->DeleteLocalRef(optionsObj);
        return;
    }

    jobject gammaObj = env->GetObjectField(optionsObj, gammaFid);
    if (gammaObj == nullptr || ClearMinecraftException(env, "Options.gamma read")) {
        if (gammaObj != nullptr) {
            env->DeleteLocalRef(gammaObj);
        }
        std::cout << "[ERROR] gamma object not found." << std::endl;
        env->DeleteLocalRef(clsOptions);
        env->DeleteLocalRef(optionsObj);
        return;
    }

    jclass clsOptionInstance = env->GetObjectClass(gammaObj);
    if (clsOptionInstance == nullptr) {
        env->DeleteLocalRef(gammaObj);
        env->DeleteLocalRef(clsOptions);
        env->DeleteLocalRef(optionsObj);
        return;
    }

    const jfieldID fidValue = env->GetFieldID(
        clsOptionInstance,
        mc_mappings::option_instance::Value.name,
        mc_mappings::option_instance::Value.signature);
    if (fidValue == nullptr || ClearMinecraftException(env, "OptionInstance.value")) {
        std::cout << "[ERROR] value field ID not found." << std::endl;
        env->DeleteLocalRef(clsOptionInstance);
        env->DeleteLocalRef(gammaObj);
        env->DeleteLocalRef(clsOptions);
        env->DeleteLocalRef(optionsObj);
        return;
    }

    jclass clsDouble = env->FindClass(mc_mappings::classes::JavaDouble);
    if (clsDouble == nullptr) {
        ClearMinecraftException(env, "FindClass(Double)");
        env->DeleteLocalRef(clsOptionInstance);
        env->DeleteLocalRef(gammaObj);
        env->DeleteLocalRef(clsOptions);
        env->DeleteLocalRef(optionsObj);
        return;
    }

    const jmethodID midDoubleInit = env->GetMethodID(
        clsDouble,
        mc_mappings::java_double::Constructor.name,
        mc_mappings::java_double::Constructor.signature);
    if (midDoubleInit == nullptr || ClearMinecraftException(env, "Double.<init>")) {
        env->DeleteLocalRef(clsDouble);
        env->DeleteLocalRef(clsOptionInstance);
        env->DeleteLocalRef(gammaObj);
        env->DeleteLocalRef(clsOptions);
        env->DeleteLocalRef(optionsObj);
        return;
    }

    jobject newGammaValue = env->NewObject(clsDouble, midDoubleInit, 1000.0);
    if (newGammaValue != nullptr && !ClearMinecraftException(env, "Double new")) {
        env->SetObjectField(gammaObj, fidValue, newGammaValue);
        ClearMinecraftException(env, "OptionInstance.value write");
        std::cout << "[INFO] FullBright enabled." << std::endl;
    }

    if (newGammaValue != nullptr) {
        env->DeleteLocalRef(newGammaValue);
    }
    env->DeleteLocalRef(clsDouble);
    env->DeleteLocalRef(clsOptionInstance);
    env->DeleteLocalRef(gammaObj);
    env->DeleteLocalRef(clsOptions);
    env->DeleteLocalRef(optionsObj);
}
