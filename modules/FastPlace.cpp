#include "FastPlace.hpp"

#include <iostream>

#include "../classes/minecraft.hpp"
#include "../jni/JniEnvironment.hpp"
#include "../jni/MinecraftMappings.hpp"

namespace {
bool ClearFastPlaceException(JNIEnv *env, const char *context) {
    if (env == nullptr || !env->ExceptionCheck()) {
        return false;
    }

    std::cout << "[FastPlace] JNI call failed at " << context << std::endl;
    env->ExceptionClear();
    return true;
}
} // namespace

FastPlace::FastPlace() {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat");
    if (env == nullptr || Minecraft::getMinecraftClass() == nullptr) {
        return;
    }

    clickDelayfield = env->GetFieldID(
        Minecraft::getMinecraftClass(),
        mc_mappings::minecraft::RightClickDelay.name,
        mc_mappings::minecraft::RightClickDelay.signature);
    if (clickDelayfield == nullptr || ClearFastPlaceException(env, "Minecraft.rightClickDelay")) {
        std::cout << "[ERROR] rightClickDelay field ID not found." << std::endl;
    }
}

void FastPlace::onUpdate() {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat Render");
    if (env == nullptr || clickDelayfield == nullptr || Minecraft::getMcInstance() == nullptr) {
        return;
    }

    env->SetIntField(Minecraft::getMcInstance(), clickDelayfield, 0);
    ClearFastPlaceException(env, "Minecraft.rightClickDelay write");
}
