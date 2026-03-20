#pragma once

#include <jni.h>

class Minecraft;

class FastBreak {
public:
    JNIEnv *env{};
    Minecraft *mc{};
    jobject obj_Gamemode{};
    jfieldID fid_destroydelay{};
    jfieldID fid_destroyprogress{};
    float break_damage{};

    explicit FastBreak(Minecraft *p_mc);
    ~FastBreak();
    void break_fast() const;
    void insta_break() const;
};
