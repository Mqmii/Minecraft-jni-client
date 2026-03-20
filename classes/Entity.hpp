#pragma once
#include <jni.h>

class Minecraft;

class Entity {
public:
    inline static jclass clsEntity{};
    jobject EntityObject{};
    JNIEnv *env{};
    Minecraft *mc{};

    explicit Entity(Minecraft *pmc);
    ~Entity();
};

class LocalPlayer {
private:
    inline static jclass playerClass;
    inline static jfieldID playerFieldID;
    JNIEnv *env{};
    Minecraft *mc{};
    jmethodID SetSprinting{};

public:
    explicit LocalPlayer(Minecraft *pmc);
    ~LocalPlayer();
    void Sprint();
    static jobject getLocalPlayerObject();

    static jclass getLocalPlayerClass();
};
