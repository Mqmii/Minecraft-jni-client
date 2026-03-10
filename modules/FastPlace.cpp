
#include "FastPlace.hpp"
#include "../classes/minecraft.hpp"
#include "../jni/MinecraftMappings.hpp"
#include <iostream>

FastPlace::FastPlace(JNIEnv *p_env) : env(p_env) {
    clickDelayfield = env->GetFieldID(
        Minecraft::getMinecraftClass(),
        mc_mappings::minecraft::RightClickDelay.name,
        mc_mappings::minecraft::RightClickDelay.signature);
    if (clickDelayfield == nullptr) {
        std::cout << "[ERROR] rightClickDelay field ID not found." << std::endl;
    }
}

void FastPlace::onUPdate() {
    env->SetIntField(Minecraft::getMcInstance(), clickDelayfield, 0);
}
