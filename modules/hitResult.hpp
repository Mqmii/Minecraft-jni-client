#pragma once

#include <jni.h>

class Minecraft;

class HitResult {
protected:
    Minecraft *mc{};
    jclass hitResultClass{};
    jfieldID hitResultFieldID{};

public:
    explicit HitResult(Minecraft *p_mc);
    virtual ~HitResult();

    [[nodiscard]] jobject getHitResultObject() const;
};

class BlockHitResult : public HitResult {
public:
    jclass clsBlockHitresult{};
    jobject obj_Blockhitresult{};

    explicit BlockHitResult(Minecraft *p_mc);
    ~BlockHitResult() override;

    void isBlock() const;
};

class EntityHitResult : public HitResult {
public:
    jclass clsEntityHitResult{};
    jclass clsEntity{};

    jobject obj_EntityHitResult{};

    jmethodID getEntityMethodID{};
    jmethodID startAttackMethodID{};
    jmethodID attackStrengthMethodID{};
    jmethodID mid_isAlive{};

    explicit EntityHitResult(Minecraft *p_mc);
    ~EntityHitResult() override;
    void isEntity() const;
    bool isAttackReady() const;
};
