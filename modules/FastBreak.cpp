#include "FastBreak.hpp"

#include "../classes/minecraft.hpp"
#include "../jni/MinecraftMappings.hpp"
FastBreak::FastBreak(JNIEnv *p_env, Minecraft *p_mc) : env(p_env), mc(p_mc) {
    jclass clsMinecraft = mc->getMinecraftClass();

    jfieldID fid_gamemode = env->GetFieldID(
        clsMinecraft,
        mc_mappings::minecraft::GameMode.name,
        mc_mappings::minecraft::GameMode.signature);

    jobject localgamemode = env->GetObjectField(mc->getMcInstance(), fid_gamemode);

    obj_Gamemode = env->NewGlobalRef(localgamemode);
    env->DeleteLocalRef(localgamemode);

    jclass cls_Gamemode = env->GetObjectClass(obj_Gamemode);

    fid_destroydelay = env->GetFieldID(
        cls_Gamemode,
        mc_mappings::game_mode::DestroyDelay.name,
        mc_mappings::game_mode::DestroyDelay.signature);
    fid_destroyprogress = env->GetFieldID(
        cls_Gamemode,
        mc_mappings::game_mode::DestroyProgress.name,
        mc_mappings::game_mode::DestroyProgress.signature);
    break_damage = env->GetFloatField(obj_Gamemode, fid_destroyprogress);
    env->DeleteLocalRef(cls_Gamemode);
}

void FastBreak::break_fast() const {
    env->SetIntField(obj_Gamemode, fid_destroydelay, -5);
}

void FastBreak::insta_break() const {
}
