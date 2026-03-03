
#include "FastPlace.hpp"
#include "../classes/minecraft.hpp"
#include <iostream>

FastPlace::FastPlace(JNIEnv *p_env) : env(p_env) {
    clickDelayfield = env->GetFieldID(Minecraft::getMinecraftClass(), "aR", "I");
    if (clickDelayfield == nullptr) {
        std::cout << "[ERROR] rightClickDelay field ID not found." << std::endl;
    }
}

void FastPlace::onUPdate() {
    env->SetIntField(Minecraft::getMcInstance(), clickDelayfield, 0);
}
