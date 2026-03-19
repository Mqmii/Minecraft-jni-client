#pragma once
#include <jni.h>

class Minecraft {
    JNIEnv *env;
    inline static jclass minecraftClass;
    inline static jobject mcInstance;

public:
    Minecraft();

    [[nodiscard]] static jobject getMcInstance();

    [[nodiscard]] static jclass getMinecraftClass();
    [[nodiscard]] JNIEnv *GetEnv() const;
    void fullBright() const;
};
