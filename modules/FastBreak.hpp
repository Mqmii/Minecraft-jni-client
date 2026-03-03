
#ifndef MINECRAFTINTERNALCHEAT_FASTBREAK_HPP
#define MINECRAFTINTERNALCHEAT_FASTBREAK_HPP

#include <jni.h>
class Minecraft;
class FastBreak {
public:
    JNIEnv *env;
    Minecraft *mc;
    jobject obj_Gamemode;
    jfieldID fid_destroydelay;
    jfieldID fid_destroyprogress;
    float break_damage;

    FastBreak(JNIEnv *p_env, Minecraft *p_mc);
    void break_fast() const;
    void insta_break() const;
};

#endif
