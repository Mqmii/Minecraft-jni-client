#pragma once

#include <jni.h>

class Minecraft;

class FastBreak {
public:
    Minecraft *mc{};
    jclass gameModeClass{};
    jfieldID gameModeFieldID{};
    jfieldID fid_destroydelay{};
    jfieldID fid_destroyprogress{};
    float break_damage{};

    explicit FastBreak(Minecraft *p_mc);
    ~FastBreak();
    void break_fast() const;
    void insta_break() const;
};
