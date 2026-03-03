#include "minecraft.hpp"

#include <iostream>
Minecraft::Minecraft(JNIEnv *p_env): env(p_env) {
    jclass localClass = env->FindClass("gfj");
    minecraftClass = (jclass) env->NewGlobalRef(localClass);
    env->DeleteLocalRef(localClass);

    jfieldID fidInstance = env->GetStaticFieldID(minecraftClass, "A", "Lgfj;");
    jobject localInstance = env->GetStaticObjectField(minecraftClass, fidInstance);

    mcInstance = env->NewGlobalRef(localInstance);
    env->DeleteLocalRef(localInstance);
}

jobject Minecraft::getMcInstance() {
    return mcInstance;
}

jclass Minecraft::getMinecraftClass() {
    return minecraftClass;
}

void Minecraft::fullBright() const {
    jfieldID fid_options = env->GetFieldID(minecraftClass, "k", "Lgfo;");
    if (fid_options == nullptr) {
        std::cout << "[ERROR] options field ID not found." << std::endl;
    }
    jobject options_obj = env->GetObjectField(mcInstance, fid_options);
    if (options_obj == nullptr) {
        std::cout << "[ERROR] options object not found." << std::endl;
    }
    jclass clsOptions = env->GetObjectClass(options_obj);
    jfieldID gamma_fid = env->GetFieldID(clsOptions, "di", "Lgfn;");
    if (gamma_fid == nullptr) {
        std::cout << "[ERROR] gamma field ID not found." << std::endl;
    }
    jobject gamma_obj = env->GetObjectField(options_obj, gamma_fid);
    if (gamma_obj == nullptr) {
        std::cout << "[ERROR] gamma object not found." << std::endl;
    }

    jclass clsOptionInstance = env->GetObjectClass(gamma_obj);
    jfieldID fid_value = env->GetFieldID(clsOptionInstance, "k", "Ljava/lang/Object;");
    if (fid_value == nullptr) {
        std::cout << "[ERROR] value field ID not found." << std::endl;
    }

    jclass clsDouble = env->FindClass("java/lang/Double");
    jmethodID mid_DoubleInit = env->GetMethodID(clsDouble, "<init>", "(D)V");
    jobject newGammaValue = env->NewObject(clsDouble, mid_DoubleInit, 1000.0);

    env->SetObjectField(gamma_obj, fid_value, newGammaValue);
    std::cout << "[INFO] FullBright enabled." << std::endl;
}
