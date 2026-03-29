#pragma once
#include <jni.h>

class Minecraft {
    inline static jclass minecraftClass;
    inline static jobject mcInstance;

public:
    Minecraft();
    ~Minecraft();

    [[nodiscard]] static jobject getMcInstance();

    [[nodiscard]] static jclass getMinecraftClass();
    void fullBright() const;
};
