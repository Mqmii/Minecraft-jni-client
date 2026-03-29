#include "hitResult.hpp"

#include <iostream>

#include "../classes/Entity.hpp"
#include "../classes/minecraft.hpp"
#include "../jni/JniEnvironment.hpp"
#include "../jni/MinecraftMappings.hpp"

namespace {
bool ClearHitResultException(JNIEnv *env, const char *context) {
    if (env == nullptr || !env->ExceptionCheck()) {
        return false;
    }

    std::cout << "[TriggerBot] JNI call failed at " << context << std::endl;
    env->ExceptionClear();
    return true;
}
} // namespace

HitResult::HitResult(Minecraft *p_mc)
    : mc(p_mc), hitResultClass(nullptr), hitResultFieldID(nullptr) {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat");
    if (env == nullptr || mc == nullptr || mc->getMinecraftClass() == nullptr) {
        return;
    }

    jclass localClass = env->FindClass(mc_mappings::classes::HitResult);
    if (localClass != nullptr) {
        hitResultClass = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
        env->DeleteLocalRef(localClass);
    } else {
        ClearHitResultException(env, "FindClass(HitResult)");
    }

    hitResultFieldID = env->GetFieldID(
        mc->getMinecraftClass(),
        mc_mappings::minecraft::HitResult.name,
        mc_mappings::minecraft::HitResult.signature);
    if (hitResultFieldID == nullptr || ClearHitResultException(env, "Minecraft.hitResult")) {
        std::cout << "[ERROR] HitResult field ID not found." << std::endl;
    }
}

HitResult::~HitResult() {
    JNIEnv *currentEnv = JniEnvironment::GetCurrentEnv();
    if (currentEnv == nullptr) {
        return;
    }

    if (hitResultClass != nullptr) {
        currentEnv->DeleteGlobalRef(hitResultClass);
        hitResultClass = nullptr;
    }
}

jobject HitResult::getHitResultObject() const {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat Render");
    if (env == nullptr || mc == nullptr || hitResultFieldID == nullptr || mc->getMcInstance() == nullptr) {
        return nullptr;
    }

    jobject hitResultObject = env->GetObjectField(mc->getMcInstance(), hitResultFieldID);
    if (ClearHitResultException(env, "Minecraft.hitResult read")) {
        if (hitResultObject != nullptr) {
            env->DeleteLocalRef(hitResultObject);
        }
        return nullptr;
    }
    return hitResultObject;
}

BlockHitResult::BlockHitResult(Minecraft *p_mc) : HitResult(p_mc) {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat");
    if (env == nullptr) {
        return;
    }

    jclass localClass = env->FindClass(mc_mappings::classes::BlockHitResult);
    if (localClass != nullptr) {
        clsBlockHitresult = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
        env->DeleteLocalRef(localClass);
    } else {
        ClearHitResultException(env, "FindClass(BlockHitResult)");
    }

    if (clsBlockHitresult == nullptr) {
        std::cout << "[ERROR] BlockHitResult class not found." << std::endl;
    }
}

BlockHitResult::~BlockHitResult() {
    JNIEnv *currentEnv = JniEnvironment::GetCurrentEnv();
    if (currentEnv == nullptr) {
        return;
    }

    if (clsBlockHitresult != nullptr) {
        currentEnv->DeleteGlobalRef(clsBlockHitresult);
        clsBlockHitresult = nullptr;
    }
}

void BlockHitResult::isBlock() const {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat Render");
    if (env == nullptr || clsBlockHitresult == nullptr) {
        return;
    }

    jobject hitResultObj = getHitResultObject();
    if (hitResultObj != nullptr && env->IsInstanceOf(hitResultObj, clsBlockHitresult)) {
        std::cout << "[DEBUG] Block target detected." << std::endl;
    } else {
        std::cout << "[DEBUG] Target is not a block." << std::endl;
    }
    if (hitResultObj != nullptr) {
        env->DeleteLocalRef(hitResultObj);
    }
}

EntityHitResult::EntityHitResult(Minecraft *p_mc) : HitResult(p_mc) {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat");
    if (env == nullptr) {
        return;
    }

    jclass localHitResultClass = env->FindClass(mc_mappings::classes::EntityHitResult);
    if (localHitResultClass == nullptr) {
        ClearHitResultException(env, "FindClass(EntityHitResult)");
        std::cout << "[ERROR] EntityHitResult class not found." << std::endl;
    } else {
        clsEntityHitResult = reinterpret_cast<jclass>(env->NewGlobalRef(localHitResultClass));
        env->DeleteLocalRef(localHitResultClass);
    }

    jclass localEntityClass = env->FindClass(mc_mappings::classes::Entity);
    if (localEntityClass == nullptr) {
        ClearHitResultException(env, "FindClass(Entity)");
        std::cout << "[ERROR] Entity class not found." << std::endl;
    } else {
        clsEntity = reinterpret_cast<jclass>(env->NewGlobalRef(localEntityClass));
        env->DeleteLocalRef(localEntityClass);
    }

    if (clsEntityHitResult != nullptr) {
        getEntityMethodID = env->GetMethodID(
            clsEntityHitResult,
            mc_mappings::entity_hit_result::GetEntity.name,
            mc_mappings::entity_hit_result::GetEntity.signature);
        if (getEntityMethodID == nullptr || ClearHitResultException(env, "EntityHitResult.getEntity")) {
            std::cout << "[ERROR] getEntity method ID not found." << std::endl;
        }
    }

    if (clsEntity != nullptr) {
        mid_isAlive = env->GetMethodID(
            clsEntity,
            mc_mappings::entity::IsAlive.name,
            mc_mappings::entity::IsAlive.signature);
        if (mid_isAlive == nullptr || ClearHitResultException(env, "Entity.isAlive")) {
            std::cout << "[ERROR] isAlive method not found." << std::endl;
        }
    }

    if (Minecraft::getMinecraftClass() != nullptr) {
        startAttackMethodID = env->GetMethodID(
            Minecraft::getMinecraftClass(),
            mc_mappings::minecraft::StartAttack.name,
            mc_mappings::minecraft::StartAttack.signature);
        if (startAttackMethodID == nullptr || ClearHitResultException(env, "Minecraft.startAttack")) {
            std::cout << "[ERROR] StartAttack method is missing!" << std::endl;
        }
    }

    if (LocalPlayer::getLocalPlayerClass() != nullptr) {
        attackStrengthMethodID = env->GetMethodID(
            LocalPlayer::getLocalPlayerClass(),
            mc_mappings::local_player::AttackStrengthScale.name,
            mc_mappings::local_player::AttackStrengthScale.signature);
        if (attackStrengthMethodID == nullptr || ClearHitResultException(env, "LocalPlayer.attackStrengthScale")) {
            std::cout << "[ERROR] getAttackStrength method not found." << std::endl;
        }
    }
}

EntityHitResult::~EntityHitResult() {
    JNIEnv *currentEnv = JniEnvironment::GetCurrentEnv();
    if (currentEnv == nullptr) {
        return;
    }

    if (clsEntityHitResult != nullptr) {
        currentEnv->DeleteGlobalRef(clsEntityHitResult);
        clsEntityHitResult = nullptr;
    }
    if (clsEntity != nullptr) {
        currentEnv->DeleteGlobalRef(clsEntity);
        clsEntity = nullptr;
    }
}

void EntityHitResult::isEntity() const {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat Render");
    if (env == nullptr || clsEntityHitResult == nullptr || getEntityMethodID == nullptr || mid_isAlive == nullptr ||
        startAttackMethodID == nullptr || Minecraft::getMcInstance() == nullptr) {
        return;
    }

    jobject hitResultObj = getHitResultObject();
    if (hitResultObj == nullptr || !env->IsInstanceOf(hitResultObj, clsEntityHitResult)) {
        if (hitResultObj != nullptr) {
            env->DeleteLocalRef(hitResultObj);
        }
        return;
    }

    jobject entity = env->CallObjectMethod(hitResultObj, getEntityMethodID);
    if (entity == nullptr || ClearHitResultException(env, "EntityHitResult.getEntity call")) {
        if (entity != nullptr) {
            env->DeleteLocalRef(entity);
        }
        env->DeleteLocalRef(hitResultObj);
        return;
    }

    const bool isAlive = env->CallBooleanMethod(entity, mid_isAlive) == JNI_TRUE;
    if (ClearHitResultException(env, "Entity.isAlive call")) {
        env->DeleteLocalRef(entity);
        env->DeleteLocalRef(hitResultObj);
        return;
    }

    if (isAlive && isAttackReady()) {
        env->CallBooleanMethod(Minecraft::getMcInstance(), startAttackMethodID);
        ClearHitResultException(env, "Minecraft.startAttack call");
    }

    env->DeleteLocalRef(entity);
    env->DeleteLocalRef(hitResultObj);
}

bool EntityHitResult::isAttackReady() const {
    JNIEnv *env = JniEnvironment::GetOrAttachCurrentEnv("JNI Cheat Render");
    if (env == nullptr || attackStrengthMethodID == nullptr) {
        return false;
    }

    jobject localPlayer = LocalPlayer::getLocalPlayerObject();
    if (localPlayer == nullptr) {
        return false;
    }

    const float strength = env->CallFloatMethod(localPlayer, attackStrengthMethodID, 0.0f);
    if (ClearHitResultException(env, "LocalPlayer.getAttackStrengthScale")) {
        env->DeleteLocalRef(localPlayer);
        return false;
    }

    env->DeleteLocalRef(localPlayer);
    return strength >= 1.0f;
}
