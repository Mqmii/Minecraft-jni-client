#pragma once

#include <jni.h>
class Minecraft;

class HitResult {
protected:
    JNIEnv *env;
    Minecraft *mc;
    jclass hitResultClass;
    jfieldID hitResultFieldID;

public:
    HitResult(JNIEnv *p_env, Minecraft *p_mc);

    [[nodiscard]] jobject getHitResultObject() const;
};

class BlockHitResult : public HitResult {
public:
    jclass clsBlockHitresult{};
    jobject obj_Blockhitresult{};

    BlockHitResult(JNIEnv *p_env, Minecraft *p_mc);

    void isBlock() const;
};

class EntityHitResult : public HitResult {
public:
    jclass clsEntityHitResult{};
    jclass clsPlayer{};
    jclass clsEntity{};

    jobject obj_EntityHitResult{};

    jmethodID getEntityMethodID{};
    jmethodID getNameMethodID{};
    jmethodID getStringMethodID{};
    jmethodID mid_isAlive{};
    EntityHitResult(JNIEnv *p_env, Minecraft *p_mc);
    void isEntity() const;
    bool isAttackReady() const;
};
