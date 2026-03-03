
#ifndef MINECRAFTWRAPPERJNI_ENTITY_HPP
#define MINECRAFTWRAPPERJNI_ENTITY_HPP
#include <jni.h>
class Minecraft;

class Entity {
public:
    jclass clsEntity{};
    jobject EntityObject{};
    JNIEnv* env;
    Minecraft *mc;
    Entity(JNIEnv* p_env,Minecraft *pmc);
};

class LocalPlayer{
private:
    inline static jclass playerClass;
    inline static jobject playerObject;
    JNIEnv* env;
    Minecraft *mc;
    jmethodID SetSprinting;
public:
    LocalPlayer(JNIEnv* p_env,Minecraft *pmc);
    void Sprint();
    static jobject getLocalPlayerObject();

    static jclass getLocalPlayerClass();
};
#endif
