#include "minecraft.hpp"

#include <iostream>

#include "../jni/JniEnvironment.hpp"
#include "../jni/MinecraftMappings.hpp"

Minecraft::Minecraft() : env(JniEnvironment::GetCurrentEnv()) {
    jclass localClass = env->FindClass(mc_mappings::classes::Minecraft);
    minecraftClass = (jclass) env->NewGlobalRef(localClass);
    env->DeleteLocalRef(localClass);

    jfieldID fidInstance = env->GetStaticFieldID(
        minecraftClass,
        mc_mappings::minecraft::Instance.name,
        mc_mappings::minecraft::Instance.signature);
    jobject localInstance = env->GetStaticObjectField(minecraftClass, fidInstance);

    mcInstance = env->NewGlobalRef(localInstance);
    env->DeleteLocalRef(localInstance);
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

JNIEnv *Minecraft::GetEnv() const {
    return env;
}

void Minecraft::fullBright() const {
    jfieldID fid_options = env->GetFieldID(
        minecraftClass,
        mc_mappings::minecraft::Options.name,
        mc_mappings::minecraft::Options.signature);
    if (fid_options == nullptr) {
        std::cout << "[ERROR] options field ID not found." << std::endl;
    }
    jobject options_obj = env->GetObjectField(mcInstance, fid_options);
    if (options_obj == nullptr) {
        std::cout << "[ERROR] options object not found." << std::endl;
    }
    jclass clsOptions = env->GetObjectClass(options_obj);
    jfieldID gamma_fid = env->GetFieldID(
        clsOptions,
        mc_mappings::options::Gamma.name,
        mc_mappings::options::Gamma.signature);
    if (gamma_fid == nullptr) {
        std::cout << "[ERROR] gamma field ID not found." << std::endl;
    }
    jobject gamma_obj = env->GetObjectField(options_obj, gamma_fid);
    if (gamma_obj == nullptr) {
        std::cout << "[ERROR] gamma object not found." << std::endl;
    }

    jclass clsOptionInstance = env->GetObjectClass(gamma_obj);
    jfieldID fid_value = env->GetFieldID(
        clsOptionInstance,
        mc_mappings::option_instance::Value.name,
        mc_mappings::option_instance::Value.signature);
    if (fid_value == nullptr) {
        std::cout << "[ERROR] value field ID not found." << std::endl;
    }

    jclass clsDouble = env->FindClass(mc_mappings::classes::JavaDouble);
    jmethodID mid_DoubleInit = env->GetMethodID(
        clsDouble,
        mc_mappings::java_double::Constructor.name,
        mc_mappings::java_double::Constructor.signature);
    jobject newGammaValue = env->NewObject(clsDouble, mid_DoubleInit, 1000.0);

    env->SetObjectField(gamma_obj, fid_value, newGammaValue);
    std::cout << "[INFO] FullBright enabled." << std::endl;
}
