#include "FastPlace.hpp"

#include <iostream>

#include "../classes/minecraft.hpp"
#include "../jni/JniEnvironment.hpp"
#include "../jni/MinecraftMappings.hpp"

FastPlace::FastPlace() : env(JniEnvironment::GetCurrentEnv()) {
    clickDelayfield = env->GetFieldID(
        Minecraft::getMinecraftClass(),
        mc_mappings::minecraft::RightClickDelay.name,
        mc_mappings::minecraft::RightClickDelay.signature);
    if (clickDelayfield == nullptr) {
        std::cout << "[ERROR] rightClickDelay field ID not found." << std::endl;
    }
}

void FastPlace::onUpdate() {
    env->SetIntField(Minecraft::getMcInstance(), clickDelayfield, 0);
}
