#include "Esp.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

#include "../classes/Entity.hpp"
#include "../imgui/imgui.h"
#include "../jni/JniEnvironment.hpp"
#include "../jni/MinecraftMappings.hpp"

namespace {
constexpr double kPi = 3.14159265358979323846;

bool ClearPendingJniException(JNIEnv *env, const char *context) {
    if (env == nullptr || !env->ExceptionCheck()) {
        return false;
    }

    std::cout << "[ESP] JNI lookup/call failed at " << context << ". ESP disabled for this session." << std::endl;
    env->ExceptionClear();
    return true;
}

bool IsFinite(double value) {
    return std::isfinite(value) != 0;
}

void AppendLookupFailure(std::string &failures, const std::string &entry) {
    std::cout << "[ESP] Missing lookup: " << entry << std::endl;
    if (!failures.empty()) {
        failures += " | ";
    }
    failures += entry;
}

void AppendClassFailure(std::string &failures, const char *label, const char *className) {
    AppendLookupFailure(failures, std::string("class ") + label + " -> " + className);
}

void AppendMemberFailure(std::string &failures, const char *kind, const char *ownerLabel, const char *name,
                         const char *signature) {
    AppendLookupFailure(failures, std::string(kind) + " " + ownerLabel + "." + name + " " + signature);
}

void SetLookupDebugState(Esp::DebugState &debugState, const char *status, const std::string &lookupDetails) {
    debugState.lastStatus = status != nullptr ? status : "";
    debugState.lookupDetails = lookupDetails;
}

double Lerp(double start, double end, float alpha) {
    return start + (end - start) * static_cast<double>(alpha);
}

struct Vec3 {
    double x{};
    double y{};
    double z{};
};

Vec3 operator-(const Vec3 &lhs, const Vec3 &rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

double Dot(const Vec3 &lhs, const Vec3 &rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 Cross(const Vec3 &lhs, const Vec3 &rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

Vec3 Normalize(const Vec3 &value) {
    const double length = std::sqrt(Dot(value, value));
    if (length <= 0.000001) {
        return {};
    }

    return {value.x / length, value.y / length, value.z / length};
}

Vec3 DirectionFromRotation(double pitchDegrees, double yawDegrees) {
    const double pitchRadians = pitchDegrees * kPi / 180.0;
    const double yawRadians = -yawDegrees * kPi / 180.0;
    const double cosPitch = std::cos(pitchRadians);

    return Normalize({
        std::sin(yawRadians) * cosPitch,
        -std::sin(pitchRadians),
        std::cos(yawRadians) * cosPitch,
    });
}

double NormalizeDegrees(double degrees) {
    double normalized = std::fmod(degrees, 360.0);
    if (normalized > 180.0) {
        normalized -= 360.0;
    } else if (normalized < -180.0) {
        normalized += 360.0;
    }
    return normalized;
}

bool WorldToScreen(const Vec3 &worldPosition, const Esp::CameraState &cameraState, float screenWidth, float screenHeight,
                   ImVec2 &screenPosition) {
    if (!cameraState.valid || screenWidth <= 0.0f || screenHeight <= 0.0f) {
        return false;
    }

    if (!IsFinite(worldPosition.x) || !IsFinite(worldPosition.y) || !IsFinite(worldPosition.z) ||
        !IsFinite(cameraState.x) || !IsFinite(cameraState.y) || !IsFinite(cameraState.z) ||
        !IsFinite(cameraState.yawDegrees) || !IsFinite(cameraState.pitchDegrees) || !IsFinite(cameraState.fovDegrees)) {
        return false;
    }

    const Vec3 relative = worldPosition - Vec3{cameraState.x, cameraState.y, cameraState.z};
    const double horizontalDistance = std::sqrt(relative.x * relative.x + relative.z * relative.z);
    if (!IsFinite(horizontalDistance) || horizontalDistance <= 0.000001) {
        return false;
    }

    const double clampedFov = std::clamp(cameraState.fovDegrees, 30.0, 170.0);
    const double aspectRatio = static_cast<double>(screenWidth) / static_cast<double>(screenHeight);
    const double verticalHalfFovRadians = (clampedFov * kPi / 180.0) * 0.5;
    const double horizontalHalfFovRadians = std::atan(std::tan(verticalHalfFovRadians) * aspectRatio);
    if (!IsFinite(verticalHalfFovRadians) || !IsFinite(horizontalHalfFovRadians) ||
        std::abs(verticalHalfFovRadians) <= 0.000001 || std::abs(horizontalHalfFovRadians) <= 0.000001) {
        return false;
    }

    const double targetYaw = std::atan2(-relative.x, relative.z) * 180.0 / kPi;
    const double targetPitch = std::atan2(relative.y, horizontalDistance) * 180.0 / kPi;
    const double cameraYaw = NormalizeDegrees(cameraState.yawDegrees);
    const double cameraPitch = NormalizeDegrees(-cameraState.pitchDegrees);

    const double deltaYaw = NormalizeDegrees(targetYaw - cameraYaw);
    const double deltaPitch = NormalizeDegrees(targetPitch - cameraPitch);
    if (!IsFinite(deltaYaw) || !IsFinite(deltaPitch)) {
        return false;
    }

    const double horizontalHalfFovDegrees = horizontalHalfFovRadians * 180.0 / kPi;
    const double verticalHalfFovDegrees = verticalHalfFovRadians * 180.0 / kPi;
    const double deltaYawRadians = deltaYaw * kPi / 180.0;
    const double deltaPitchRadians = deltaPitch * kPi / 180.0;

    double normalizedX = deltaYaw / horizontalHalfFovDegrees;
    double normalizedY = deltaPitch / verticalHalfFovDegrees;

    // Use perspective-correct tangent projection for targets near the visible frustum.
    // This removes the left/right drift that linear angle mapping causes near screen edges.
    if (std::abs(deltaYaw) < 89.0 && std::abs(deltaPitch) < 89.0) {
        const double tangentX = std::tan(deltaYawRadians) / std::tan(horizontalHalfFovRadians);
        const double tangentY = std::tan(deltaPitchRadians) / std::tan(verticalHalfFovRadians);
        if (IsFinite(tangentX) && IsFinite(tangentY)) {
            normalizedX = tangentX;
            normalizedY = tangentY;
        }
    }

    if (!IsFinite(normalizedX) || !IsFinite(normalizedY)) {
        return false;
    }

    const double absX = std::abs(normalizedX);
    const double absY = std::abs(normalizedY);
    double clampedX = normalizedX;
    double clampedY = normalizedY;

    // Keep off-screen targets visible by projecting them onto the nearest screen edge.
    if (absX > 1.0 || absY > 1.0) {
        const double scale = 1.0 / std::max(absX, absY);
        clampedX *= scale;
        clampedY *= scale;
    }

    const float screenPadding = 18.0f;
    screenPosition.x = static_cast<float>((clampedX + 1.0) * 0.5 * screenWidth);
    screenPosition.y = static_cast<float>((1.0 - clampedY) * 0.5 * screenHeight);
    screenPosition.x = std::clamp(screenPosition.x, screenPadding, screenWidth - screenPadding);
    screenPosition.y = std::clamp(screenPosition.y, screenPadding, screenHeight - screenPadding);
    return true;
}
} // namespace

Esp::Esp() {
    JNIEnv *env = JniEnvironment::GetCurrentEnv();
    std::string lookupFailures;
    if (env == nullptr || Minecraft::getMinecraftClass() == nullptr || Entity::clsEntity == nullptr) {
        std::cout << "[ESP] JNI environment or required classes are unavailable." << std::endl;
        std::scoped_lock lock(stateMutex_);
        SetLookupDebugState(debugState_, "JNI environment or base classes unavailable", {});
        return;
    }

    levelField = env->GetFieldID(
        Minecraft::getMinecraftClass(),
        mc_mappings::minecraft::Level.name,
        mc_mappings::minecraft::Level.signature);
    if (levelField == nullptr || ClearPendingJniException(env, "Minecraft.level")) {
        AppendMemberFailure(lookupFailures, "field", "Minecraft", mc_mappings::minecraft::Level.name,
                            mc_mappings::minecraft::Level.signature);
        std::cout << "[Error] Minecraft.level field cant be found." << std::endl;
        std::scoped_lock lock(stateMutex_);
        SetLookupDebugState(debugState_, "Minecraft.level field missing", lookupFailures);
        return;
    }

    jclass localLevelClass = env->FindClass(mc_mappings::classes::ClientLevel);
    if (localLevelClass != nullptr) {
        levelClass = reinterpret_cast<jclass>(env->NewGlobalRef(localLevelClass));
        env->DeleteLocalRef(localLevelClass);
    } else {
        AppendClassFailure(lookupFailures, "ClientLevel", mc_mappings::classes::ClientLevel);
    }

    jclass localListClass = env->FindClass(mc_mappings::classes::JavaList);
    if (localListClass != nullptr) {
        listClass = reinterpret_cast<jclass>(env->NewGlobalRef(localListClass));
        env->DeleteLocalRef(localListClass);
    } else {
        AppendClassFailure(lookupFailures, "java.util.List", mc_mappings::classes::JavaList);
    }

    jclass localDeltaTrackerTimerClass = env->FindClass(mc_mappings::classes::DeltaTrackerTimer);
    if (localDeltaTrackerTimerClass != nullptr) {
        deltaTrackerTimerClass = reinterpret_cast<jclass>(env->NewGlobalRef(localDeltaTrackerTimerClass));
        env->DeleteLocalRef(localDeltaTrackerTimerClass);
    } else {
        AppendClassFailure(lookupFailures, "DeltaTracker.Timer", mc_mappings::classes::DeltaTrackerTimer);
    }

    if (levelClass == nullptr || listClass == nullptr || deltaTrackerTimerClass == nullptr ||
        ClearPendingJniException(env, "ESP class lookup")) {
        std::cout << "[ESP] Required classes could not be resolved." << std::endl;
        std::scoped_lock lock(stateMutex_);
        SetLookupDebugState(debugState_, "Required ESP classes missing", lookupFailures);
        return;
    }

    jclass localOptionsClass = env->FindClass(mc_mappings::classes::Options);
    if (localOptionsClass != nullptr) {
        optionsClass = reinterpret_cast<jclass>(env->NewGlobalRef(localOptionsClass));
        env->DeleteLocalRef(localOptionsClass);
    }

    jclass localOptionInstanceClass = env->FindClass(mc_mappings::classes::OptionInstance);
    if (localOptionInstanceClass != nullptr) {
        optionInstanceClass = reinterpret_cast<jclass>(env->NewGlobalRef(localOptionInstanceClass));
        env->DeleteLocalRef(localOptionInstanceClass);
    }

    jclass localNumberClass = env->FindClass(mc_mappings::classes::JavaNumber);
    if (localNumberClass != nullptr) {
        numberClass = reinterpret_cast<jclass>(env->NewGlobalRef(localNumberClass));
        env->DeleteLocalRef(localNumberClass);
    }

    getPlayersMethodID = env->GetMethodID(levelClass, mc_mappings::client_level::Players.name,
                                          mc_mappings::client_level::Players.signature);
    listSizeMethodID = env->GetMethodID(listClass, mc_mappings::java_list::Size.name,
                                        mc_mappings::java_list::Size.signature);
    listGetMethodID = env->GetMethodID(listClass, mc_mappings::java_list::Get.name,
                                       mc_mappings::java_list::Get.signature);

    getXMethodID = env->GetMethodID(Entity::clsEntity, mc_mappings::entity::GetX.name,
                                    mc_mappings::entity::GetX.signature);
    getYMethodID = env->GetMethodID(Entity::clsEntity, mc_mappings::entity::GetY.name,
                                    mc_mappings::entity::GetY.signature);
    getEyeYMethodID = env->GetMethodID(Entity::clsEntity, mc_mappings::entity::GetEyeY.name,
                                       mc_mappings::entity::GetEyeY.signature);
    getZMethodID = env->GetMethodID(Entity::clsEntity, mc_mappings::entity::GetZ.name,
                                    mc_mappings::entity::GetZ.signature);
    getYawMethodID = env->GetMethodID(Entity::clsEntity, mc_mappings::entity::GetYaw.name,
                                      mc_mappings::entity::GetYaw.signature);
    getPitchMethodID = env->GetMethodID(Entity::clsEntity, mc_mappings::entity::GetPitch.name,
                                        mc_mappings::entity::GetPitch.signature);
    oldXFieldID = env->GetFieldID(Entity::clsEntity, mc_mappings::entity::OldX.name,
                                  mc_mappings::entity::OldX.signature);
    oldYFieldID = env->GetFieldID(Entity::clsEntity, mc_mappings::entity::OldY.name,
                                  mc_mappings::entity::OldY.signature);
    oldZFieldID = env->GetFieldID(Entity::clsEntity, mc_mappings::entity::OldZ.name,
                                  mc_mappings::entity::OldZ.signature);

    if (getPlayersMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "ClientLevel", mc_mappings::client_level::Players.name,
                            mc_mappings::client_level::Players.signature);
    }
    if (listSizeMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "List", mc_mappings::java_list::Size.name,
                            mc_mappings::java_list::Size.signature);
    }
    if (listGetMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "List", mc_mappings::java_list::Get.name,
                            mc_mappings::java_list::Get.signature);
    }
    if (getXMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "Entity", mc_mappings::entity::GetX.name,
                            mc_mappings::entity::GetX.signature);
    }
    if (getYMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "Entity", mc_mappings::entity::GetY.name,
                            mc_mappings::entity::GetY.signature);
    }
    if (getEyeYMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "Entity", mc_mappings::entity::GetEyeY.name,
                            mc_mappings::entity::GetEyeY.signature);
    }
    if (getZMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "Entity", mc_mappings::entity::GetZ.name,
                            mc_mappings::entity::GetZ.signature);
    }
    if (getYawMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "Entity", mc_mappings::entity::GetYaw.name,
                            mc_mappings::entity::GetYaw.signature);
    }
    if (getPitchMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "Entity", mc_mappings::entity::GetPitch.name,
                            mc_mappings::entity::GetPitch.signature);
    }
    if (oldXFieldID == nullptr) {
        AppendMemberFailure(lookupFailures, "field", "Entity", mc_mappings::entity::OldX.name,
                            mc_mappings::entity::OldX.signature);
    }
    if (oldYFieldID == nullptr) {
        AppendMemberFailure(lookupFailures, "field", "Entity", mc_mappings::entity::OldY.name,
                            mc_mappings::entity::OldY.signature);
    }
    if (oldZFieldID == nullptr) {
        AppendMemberFailure(lookupFailures, "field", "Entity", mc_mappings::entity::OldZ.name,
                            mc_mappings::entity::OldZ.signature);
    }

    if (getPlayersMethodID == nullptr || listSizeMethodID == nullptr || listGetMethodID == nullptr ||
        getXMethodID == nullptr || getYMethodID == nullptr || getEyeYMethodID == nullptr || getZMethodID == nullptr ||
        getYawMethodID == nullptr || getPitchMethodID == nullptr ||
        oldXFieldID == nullptr || oldYFieldID == nullptr || oldZFieldID == nullptr ||
        ClearPendingJniException(env, "required ESP methods")) {
        std::cout << "[ESP] Required level/entity methods could not be resolved." << std::endl;
        std::scoped_lock lock(stateMutex_);
        SetLookupDebugState(debugState_, "Required ESP methods missing", lookupFailures);
        return;
    }

    optionsField = env->GetFieldID(
        Minecraft::getMinecraftClass(),
        mc_mappings::minecraft::Options.name,
        mc_mappings::minecraft::Options.signature);
    if (optionsClass != nullptr) {
        fovField = env->GetFieldID(
            optionsClass,
            mc_mappings::options::Fov.name,
            mc_mappings::options::Fov.signature);
    }
    if (optionInstanceClass != nullptr) {
        optionValueField = env->GetFieldID(
            optionInstanceClass,
            mc_mappings::option_instance::Value.name,
            mc_mappings::option_instance::Value.signature);
    }
    if (numberClass != nullptr) {
        numberDoubleValueMethodID = env->GetMethodID(numberClass, mc_mappings::java_number::DoubleValue.name,
                                                     mc_mappings::java_number::DoubleValue.signature);
    }
    deltaTrackerField = env->GetFieldID(Minecraft::getMinecraftClass(), mc_mappings::minecraft::DeltaTracker.name,
                                        mc_mappings::minecraft::DeltaTracker.signature);
    getGameTimeDeltaPartialTickMethodID = env->GetMethodID(
        deltaTrackerTimerClass,
        mc_mappings::delta_tracker_timer::GetGameTimeDeltaPartialTick.name,
        mc_mappings::delta_tracker_timer::GetGameTimeDeltaPartialTick.signature);

    if (deltaTrackerField == nullptr) {
        AppendMemberFailure(lookupFailures, "field", "Minecraft", mc_mappings::minecraft::DeltaTracker.name,
                            mc_mappings::minecraft::DeltaTracker.signature);
    }
    if (getGameTimeDeltaPartialTickMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "DeltaTracker.Timer",
                            mc_mappings::delta_tracker_timer::GetGameTimeDeltaPartialTick.name,
                            mc_mappings::delta_tracker_timer::GetGameTimeDeltaPartialTick.signature);
    }

    ClearPendingJniException(env, "optional ESP FOV lookup");
    initialized_ = true;
    {
        std::scoped_lock lock(stateMutex_);
        debugState_.initialized = true;
        SetLookupDebugState(debugState_, "ESP initialized", lookupFailures);
    }
    std::cout << "[+] ESP camera/FOV initialization ready." << std::endl;
}

bool Esp::IsInitialized() const {
    return initialized_;
}

void Esp::SetStatusLocked(const char *status) {
    debugState_.lastStatus = status != nullptr ? status : "";
}

void Esp::SetStatus(const char *status) {
    std::scoped_lock lock(stateMutex_);
    SetStatusLocked(status);
}

double Esp::ReadCurrentFov(JNIEnv *env) const {
    if (optionsField == nullptr || fovField == nullptr || optionValueField == nullptr || numberDoubleValueMethodID == nullptr) {
        return 90.0;
    }

    jobject optionsObj = env->GetObjectField(Minecraft::getMcInstance(), optionsField);
    if (optionsObj == nullptr) {
        return 90.0;
    }

    jobject fovOptionObj = env->GetObjectField(optionsObj, fovField);
    env->DeleteLocalRef(optionsObj);
    if (fovOptionObj == nullptr) {
        return 90.0;
    }

    jobject fovValueObj = env->GetObjectField(fovOptionObj, optionValueField);
    env->DeleteLocalRef(fovOptionObj);
    if (fovValueObj == nullptr) {
        return 90.0;
    }

    const double fovValue = env->CallDoubleMethod(fovValueObj, numberDoubleValueMethodID);
    env->DeleteLocalRef(fovValueObj);
    if (ClearPendingJniException(env, "OptionInstance.value/doubleValue") || !IsFinite(fovValue)) {
        return 90.0;
    }
    return fovValue;
}

float Esp::ReadCurrentFrameTime(JNIEnv *env) const {
    if (env == nullptr || deltaTrackerField == nullptr || getGameTimeDeltaPartialTickMethodID == nullptr) {
        return 1.0f;
    }

    jobject deltaTrackerObj = env->GetObjectField(Minecraft::getMcInstance(), deltaTrackerField);
    if (deltaTrackerObj == nullptr) {
        return 1.0f;
    }

    const jfloat frameTime = env->CallFloatMethod(deltaTrackerObj, getGameTimeDeltaPartialTickMethodID, JNI_TRUE);
    env->DeleteLocalRef(deltaTrackerObj);
    if (ClearPendingJniException(env, "DeltaTracker.Timer.getGameTimeDeltaPartialTick") || !std::isfinite(frameTime)) {
        return 1.0f;
    }

    return std::clamp(static_cast<float>(frameTime), 0.0f, 1.0f);
}

void Esp::Tick() {
    if (!initialized_) {
        return;
    }

    JNIEnv *env = JniEnvironment::GetCurrentEnv();
    if (env == nullptr) {
        SetStatus("Tick skipped: JNIEnv unavailable");
        return;
    }
    std::vector<Target> updatedTargets;
    CameraState updatedCamera{};

    jobject localPlayer = LocalPlayer::getLocalPlayerObject();
    bool localPlayerValid = false;
    bool levelValid = false;
    bool playerListValid = false;
    int playerCountValue = 0;
    if (localPlayer != nullptr) {
        localPlayerValid = true;
        updatedCamera.previousX = env->GetDoubleField(localPlayer, oldXFieldID);
        updatedCamera.previousY = env->GetDoubleField(localPlayer, oldYFieldID);
        updatedCamera.previousZ = env->GetDoubleField(localPlayer, oldZFieldID);
        updatedCamera.x = env->CallDoubleMethod(localPlayer, getXMethodID);
        updatedCamera.y = env->CallDoubleMethod(localPlayer, getYMethodID);
        updatedCamera.z = env->CallDoubleMethod(localPlayer, getZMethodID);
        const double currentEyeY = env->CallDoubleMethod(localPlayer, getEyeYMethodID);
        updatedCamera.yawDegrees = NormalizeDegrees(env->CallFloatMethod(localPlayer, getYawMethodID));
        updatedCamera.pitchDegrees = NormalizeDegrees(env->CallFloatMethod(localPlayer, getPitchMethodID));
        if (ClearPendingJniException(env, "camera reads")) {
            updatedCamera = {};
            localPlayerValid = false;
        } else {
            updatedCamera.eyeHeightOffset = currentEyeY - updatedCamera.y;
        }
        updatedCamera.fovDegrees = ReadCurrentFov(env);
        updatedCamera.valid = IsFinite(updatedCamera.previousX) && IsFinite(updatedCamera.previousY) &&
                              IsFinite(updatedCamera.previousZ) && IsFinite(updatedCamera.x) && IsFinite(updatedCamera.y) &&
                              IsFinite(updatedCamera.z) && IsFinite(updatedCamera.eyeHeightOffset) &&
                              IsFinite(updatedCamera.yawDegrees) && IsFinite(updatedCamera.pitchDegrees) &&
                              IsFinite(updatedCamera.fovDegrees);
    }

    jobject levelObj = env->GetObjectField(Minecraft::getMcInstance(), levelField);
    if (levelObj == nullptr || ClearPendingJniException(env, "Minecraft.level read")) {
        std::scoped_lock lock(stateMutex_);
        targets_.clear();
        cameraState_ = updatedCamera;
        debugState_.localPlayerValid = localPlayerValid;
        debugState_.levelValid = false;
        debugState_.playerListValid = false;
        debugState_.cameraValid = updatedCamera.valid;
        debugState_.playerCount = 0;
        debugState_.targetCount = 0;
        debugState_.cameraX = updatedCamera.x;
        debugState_.cameraY = updatedCamera.y;
        debugState_.cameraZ = updatedCamera.z;
        debugState_.yawDegrees = updatedCamera.yawDegrees;
        debugState_.pitchDegrees = updatedCamera.pitchDegrees;
        debugState_.fovDegrees = updatedCamera.fovDegrees;
        SetStatusLocked("Tick: level unavailable");
        if (localPlayer != nullptr) {
            env->DeleteLocalRef(localPlayer);
        }
        return;
    }
    levelValid = true;

    jobject playerList = env->CallObjectMethod(levelObj, getPlayersMethodID);
    if (playerList == nullptr || ClearPendingJniException(env, "ClientLevel.players")) {
        env->DeleteLocalRef(levelObj);
        std::scoped_lock lock(stateMutex_);
        targets_.clear();
        cameraState_ = updatedCamera;
        debugState_.localPlayerValid = localPlayerValid;
        debugState_.levelValid = levelValid;
        debugState_.playerListValid = false;
        debugState_.cameraValid = updatedCamera.valid;
        debugState_.playerCount = 0;
        debugState_.targetCount = 0;
        debugState_.cameraX = updatedCamera.x;
        debugState_.cameraY = updatedCamera.y;
        debugState_.cameraZ = updatedCamera.z;
        debugState_.yawDegrees = updatedCamera.yawDegrees;
        debugState_.pitchDegrees = updatedCamera.pitchDegrees;
        debugState_.fovDegrees = updatedCamera.fovDegrees;
        SetStatusLocked("Tick: player list unavailable");
        if (localPlayer != nullptr) {
            env->DeleteLocalRef(localPlayer);
        }
        return;
    }
    playerListValid = true;

    const jint playerCount = env->CallIntMethod(playerList, listSizeMethodID);
    playerCountValue = static_cast<int>(playerCount);
    updatedTargets.reserve(static_cast<size_t>(playerCount));

    for (jint i = 0; i < playerCount; ++i) {
        jobject playerObj = env->CallObjectMethod(playerList, listGetMethodID, i);
        if (playerObj == nullptr) {
            continue;
        }

        if (localPlayer != nullptr && env->IsSameObject(playerObj, localPlayer)) {
            env->DeleteLocalRef(playerObj);
            continue;
        }

        updatedTargets.push_back(Target{
            env->GetDoubleField(playerObj, oldXFieldID),
            env->GetDoubleField(playerObj, oldYFieldID),
            env->GetDoubleField(playerObj, oldZFieldID),
            env->CallDoubleMethod(playerObj, getXMethodID),
            env->CallDoubleMethod(playerObj, getYMethodID),
            env->CallDoubleMethod(playerObj, getZMethodID),
            0.0,
        });
        const double currentEyeY = env->CallDoubleMethod(playerObj, getEyeYMethodID);
        if (ClearPendingJniException(env, "target position reads")) {
            updatedTargets.clear();
            env->DeleteLocalRef(playerObj);
            playerListValid = false;
            break;
        }

        Target &target = updatedTargets.back();
        target.eyeHeightOffset = currentEyeY - target.y;

        env->DeleteLocalRef(playerObj);
    }

    env->DeleteLocalRef(playerList);
    env->DeleteLocalRef(levelObj);
    if (localPlayer != nullptr) {
        env->DeleteLocalRef(localPlayer);
    }

    std::scoped_lock lock(stateMutex_);
    targets_.swap(updatedTargets);
    cameraState_ = updatedCamera;
    debugState_.localPlayerValid = localPlayerValid;
    debugState_.levelValid = levelValid;
    debugState_.playerListValid = playerListValid;
    debugState_.cameraValid = updatedCamera.valid;
    debugState_.playerCount = playerCountValue;
    debugState_.targetCount = static_cast<int>(targets_.size());
    debugState_.cameraX = updatedCamera.x;
    debugState_.cameraY = updatedCamera.y;
    debugState_.cameraZ = updatedCamera.z;
    debugState_.yawDegrees = updatedCamera.yawDegrees;
    debugState_.pitchDegrees = updatedCamera.pitchDegrees;
    debugState_.fovDegrees = updatedCamera.fovDegrees;
    SetStatusLocked(updatedCamera.valid ? "Tick OK" : "Tick: camera invalid");
}

void Esp::RenderOverlay() const {
    std::vector<Target> snapshot;
    CameraState cameraSnapshot{};
    DebugState debugSnapshot{};
    {
        std::scoped_lock lock(stateMutex_);
        snapshot = targets_;
        cameraSnapshot = cameraState_;
        debugSnapshot = debugState_;
    }

    JNIEnv *env = JniEnvironment::GetCurrentEnv();
    const float frameTime = ReadCurrentFrameTime(env);
    cameraSnapshot.x = Lerp(cameraSnapshot.previousX, cameraSnapshot.x, frameTime);
    cameraSnapshot.y = Lerp(cameraSnapshot.previousY, cameraSnapshot.y, frameTime) + cameraSnapshot.eyeHeightOffset;
    cameraSnapshot.z = Lerp(cameraSnapshot.previousZ, cameraSnapshot.z, frameTime);

    ImDrawList *drawList = ImGui::GetBackgroundDrawList();
    const ImGuiIO &io = ImGui::GetIO();
    const ImVec2 tracerStart(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 textPosition(15.0f, 15.0f);
    const ImU32 textColor = IM_COL32(255, 255, 255, 230);
    char lineBuffer[256]{};

    std::snprintf(lineBuffer, sizeof(lineBuffer), "ESP init=%d local=%d level=%d list=%d cam=%d",
                  debugSnapshot.initialized ? 1 : 0,
                  debugSnapshot.localPlayerValid ? 1 : 0,
                  debugSnapshot.levelValid ? 1 : 0,
                  debugSnapshot.playerListValid ? 1 : 0,
                  debugSnapshot.cameraValid ? 1 : 0);
    drawList->AddText(textPosition, textColor, lineBuffer);
    textPosition.y += 16.0f;

    std::snprintf(lineBuffer, sizeof(lineBuffer), "count players=%d targets=%d", debugSnapshot.playerCount,
                  debugSnapshot.targetCount);
    drawList->AddText(textPosition, textColor, lineBuffer);
    textPosition.y += 16.0f;

    std::snprintf(lineBuffer, sizeof(lineBuffer), "cam xyz=%.2f %.2f %.2f", debugSnapshot.cameraX, debugSnapshot.cameraY,
                  debugSnapshot.cameraZ);
    drawList->AddText(textPosition, textColor, lineBuffer);
    textPosition.y += 16.0f;

    std::snprintf(lineBuffer, sizeof(lineBuffer), "yaw/pitch/fov=%.2f %.2f %.2f", debugSnapshot.yawDegrees,
                  debugSnapshot.pitchDegrees, debugSnapshot.fovDegrees);
    drawList->AddText(textPosition, textColor, lineBuffer);
    textPosition.y += 16.0f;

    std::snprintf(lineBuffer, sizeof(lineBuffer), "frameTime=%.2f", frameTime);
    drawList->AddText(textPosition, textColor, lineBuffer);
    textPosition.y += 16.0f;

    drawList->AddText(textPosition, IM_COL32(255, 210, 120, 230), debugSnapshot.lastStatus.c_str());
    textPosition.y += 16.0f;
    if (!debugSnapshot.lookupDetails.empty()) {
        drawList->AddText(textPosition, IM_COL32(255, 150, 150, 230), debugSnapshot.lookupDetails.c_str());
    }

    if (snapshot.empty() || !cameraSnapshot.valid) {
        return;
    }

    for (const Target &target : snapshot) {
        const double interpolatedX = Lerp(target.previousX, target.x, frameTime);
        const double interpolatedY = Lerp(target.previousY, target.y, frameTime) + target.eyeHeightOffset;
        const double interpolatedZ = Lerp(target.previousZ, target.z, frameTime);
        ImVec2 screenPosition{};
        if (!WorldToScreen({interpolatedX, interpolatedY - 0.60, interpolatedZ}, cameraSnapshot, io.DisplaySize.x, io.DisplaySize.y,
                           screenPosition)) {
            continue;
        }

        if (!IsFinite(screenPosition.x) || !IsFinite(screenPosition.y)) {
            continue;
        }

        drawList->AddLine(tracerStart, screenPosition, IM_COL32(255, 70, 70, 220), 1.5f);
        drawList->AddCircleFilled(screenPosition, 3.0f, IM_COL32(255, 70, 70, 220));
    }
}
