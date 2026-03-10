#include "hitResult.hpp"
#include <iostream>
#include "../classes/minecraft.hpp"
#include "click.hpp"
#include "../classes/Entity.hpp"
#include "../jni/MinecraftMappings.hpp"
HitResult::HitResult(JNIEnv *p_env, Minecraft *p_mc) : env(p_env), mc(p_mc), hitResultClass(nullptr),
                                                       hitResultFieldID(nullptr) {
    jclass localClass = env->FindClass(mc_mappings::classes::HitResult);
    hitResultClass = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
    env->DeleteLocalRef(localClass);

    hitResultFieldID = env->GetFieldID(
        mc->getMinecraftClass(),
        mc_mappings::minecraft::HitResult.name,
        mc_mappings::minecraft::HitResult.signature);
    if (hitResultFieldID == nullptr) {
        std::cout << "[ERROR] HitResult field ID not found." << std::endl;
    }
}

jobject HitResult::getHitResultObject() const {
    jobject localinstance = env->GetObjectField(mc->getMcInstance(), hitResultFieldID);
    return localinstance;
}

BlockHitResult::BlockHitResult(JNIEnv *p_env, Minecraft *p_mc) : HitResult(p_env, p_mc) {
    jclass localclass = env->FindClass(mc_mappings::classes::BlockHitResult);
    clsBlockHitresult = reinterpret_cast<jclass>(env->NewGlobalRef(localclass));
    env->DeleteLocalRef(localclass);
    if (clsBlockHitresult == nullptr) {
        std::cout << "[ERROR] BlockHitResult class not found." << std::endl;
    }
}

void BlockHitResult::isBlock() const {
    jobject hitresultobj = getHitResultObject();
    if (env->IsInstanceOf(hitresultobj, clsBlockHitresult)) {
        std::cout << "[DEBUG] Block target detected." << std::endl;
    } else {
        std::cout << "[DEBUG] Target is not a block." << std::endl;
    }
    env->DeleteLocalRef(hitresultobj);
}

EntityHitResult::EntityHitResult(JNIEnv *p_env, Minecraft *p_mc) : HitResult(p_env, p_mc) {
    jclass localclass = env->FindClass(mc_mappings::classes::EntityHitResult);
    if (localclass == nullptr) {
        std::cout << "[ERROR] EntityHitResult class not found." << std::endl;
    }
    clsEntityHitResult = reinterpret_cast<jclass>(env->NewGlobalRef(localclass));
    env->DeleteLocalRef(localclass);
    getEntityMethodID = env->GetMethodID(
        clsEntityHitResult,
        mc_mappings::entity_hit_result::GetEntity.name,
        mc_mappings::entity_hit_result::GetEntity.signature);
    if (getEntityMethodID == nullptr)std::cout << "[ERROR] getEntity method ID not found." << std::endl;

    jclass localPlayer = env->FindClass(mc_mappings::classes::Player);
    if (localPlayer == nullptr) std::cout << "[ERROR] Player class not found." << std::endl;
    clsPlayer = reinterpret_cast<jclass>(env->NewGlobalRef(localPlayer));
    env->DeleteLocalRef(localPlayer);
    jclass localEntity = env->FindClass(mc_mappings::classes::Entity);
    if (!localEntity) std::cout << "[ERROR] Entity class not found." << std::endl;
    getNameMethodID = env->GetMethodID(
        localEntity,
        mc_mappings::entity::GetName.name,
        mc_mappings::entity::GetName.signature);
    if (getNameMethodID == nullptr) std::cout << "[ERROR] getName method ID not found." << std::endl;
    env->DeleteLocalRef(localEntity);

    jclass localComponent = env->FindClass(mc_mappings::classes::Component);
    if (localComponent == nullptr) std::cout << "[ERROR] Component class not found." << std::endl;
    getStringMethodID = env->GetMethodID(
        localComponent,
        mc_mappings::component::GetString.name,
        mc_mappings::component::GetString.signature);
    if (getStringMethodID == nullptr) std::cout << "[ERROR] getString method ID not found." << std::endl;
    jclass localentitycls = env->FindClass(mc_mappings::classes::Entity);
    if (localentitycls == nullptr) {
        std::cout << "[ERROR] Entity class not found." << std::endl;
    }
    clsEntity = reinterpret_cast<jclass>(env->NewGlobalRef(localentitycls));
    mid_isAlive = env->GetMethodID(
        clsEntity,
        mc_mappings::entity::IsAlive.name,
        mc_mappings::entity::IsAlive.signature);
    if (mid_isAlive == nullptr) {
        std::cout << "[ERROR] isAlive method not found." << std::endl;
    }
    env->DeleteLocalRef(localComponent);

}

void EntityHitResult::isEntity() const {
    jobject obj_hitresult = getHitResultObject();
    if (!env->IsInstanceOf(obj_hitresult,clsEntityHitResult)) {
        env->DeleteLocalRef(obj_hitresult);
        return;
    }
    jobject entity = env->CallObjectMethod(obj_hitresult,getEntityMethodID);
    if (entity == nullptr) {
        env->DeleteLocalRef(obj_hitresult);
        return;
    }
    bool isalive = env->CallBooleanMethod(entity,mid_isAlive);

    if (!env->IsInstanceOf(entity,clsPlayer)) {
        if (isAttackReady() and isalive) {
            Click::LeftClick();
        }
        env->DeleteLocalRef(entity);
        env->DeleteLocalRef(obj_hitresult);
        return;
    }

    jobject componentObj = (env->CallObjectMethod(entity,getNameMethodID));
    if (componentObj == nullptr) {
        std::cout << "[ERROR] Component object not found." << std::endl;
        env->DeleteLocalRef(entity);
        env->DeleteLocalRef(obj_hitresult);
        return;
    }

    auto nameJstr = reinterpret_cast<jstring>(env->CallObjectMethod(componentObj,getStringMethodID));
    if (nameJstr == nullptr) std::cout << "[ERROR] Player name string is null." << std::endl;
    const char *nameChars = env->GetStringUTFChars(nameJstr,nullptr);
    std::string playerName(nameChars);
    env->ReleaseStringUTFChars(nameJstr,nameChars);
    std::cout << "[DEBUG] Player name: " << playerName << std::endl;
    if (isAttackReady() and isalive) {
        Click::LeftClick();
    }
    env->DeleteLocalRef(entity);
    env->DeleteLocalRef(componentObj);
    env->DeleteLocalRef(obj_hitresult);
    env->DeleteLocalRef(nameJstr);
}
bool EntityHitResult::isAttackReady() const {
    jmethodID mid_GetStregth = env->GetMethodID(
        LocalPlayer::getLocalPlayerClass(),
        mc_mappings::local_player::AttackStrengthScale.name,
        mc_mappings::local_player::AttackStrengthScale.signature);
    if (mid_GetStregth == nullptr) {
        std::cout << "[ERROR] getAttackStrength method not found." << std::endl;
        return false;
    }
    float strength = env->CallFloatMethod(LocalPlayer::getLocalPlayerObject(),mid_GetStregth,0.0f);
    if (strength >= 1.0f) {
        return true;
    }
    return false;
}
