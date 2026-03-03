#pragma once
#include <jni.h>

class Minecraft {
    JNIEnv *env;
    inline static jclass minecraftClass;
    inline static jobject mcInstance;

public:
    explicit Minecraft(JNIEnv *p_env);

    [[nodiscard]] static jobject getMcInstance();

    [[nodiscard]] static jclass getMinecraftClass();
    void fullBright() const;
};
