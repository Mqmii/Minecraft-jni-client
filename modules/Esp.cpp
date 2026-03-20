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

ImU32 ColorFromArray(const float *color, ImU32 fallback) {
    if (color == nullptr) {
        return fallback;
    }

    return ImGui::ColorConvertFloat4ToU32(ImVec4(
        std::clamp(color[0], 0.0f, 1.0f),
        std::clamp(color[1], 0.0f, 1.0f),
        std::clamp(color[2], 0.0f, 1.0f),
        std::clamp(color[3], 0.0f, 1.0f)));
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
                   ImVec2 &screenPosition, bool clampOffscreen = true) {
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

    if (!clampOffscreen && (absX > 1.0 || absY > 1.0)) {
        return false;
    }

    // Keep off-screen targets visible by projecting them onto the nearest screen edge.
    if (clampOffscreen && (absX > 1.0 || absY > 1.0)) {
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

    jclass localGameRendererClass = env->FindClass(mc_mappings::classes::GameRenderer);
    if (localGameRendererClass != nullptr) {
        gameRendererClass = reinterpret_cast<jclass>(env->NewGlobalRef(localGameRendererClass));
        env->DeleteLocalRef(localGameRendererClass);
    } else {
        AppendClassFailure(lookupFailures, "GameRenderer", mc_mappings::classes::GameRenderer);
    }

    jclass localCameraClass = env->FindClass(mc_mappings::classes::Camera);
    if (localCameraClass != nullptr) {
        cameraClass = reinterpret_cast<jclass>(env->NewGlobalRef(localCameraClass));
        env->DeleteLocalRef(localCameraClass);
    } else {
        AppendClassFailure(lookupFailures, "Camera", mc_mappings::classes::Camera);
    }

    jclass localVec3Class = env->FindClass(mc_mappings::classes::Vec3);
    if (localVec3Class != nullptr) {
        vec3Class = reinterpret_cast<jclass>(env->NewGlobalRef(localVec3Class));
        env->DeleteLocalRef(localVec3Class);
    } else {
        AppendClassFailure(lookupFailures, "Vec3", mc_mappings::classes::Vec3);
    }

    jclass localPlayerClass = env->FindClass(mc_mappings::classes::Player);
    if (localPlayerClass != nullptr) {
        playerClass = reinterpret_cast<jclass>(env->NewGlobalRef(localPlayerClass));
        env->DeleteLocalRef(localPlayerClass);
    }

    if (levelClass == nullptr || listClass == nullptr || deltaTrackerTimerClass == nullptr ||
        gameRendererClass == nullptr || cameraClass == nullptr || vec3Class == nullptr ||
        ClearPendingJniException(env, "ESP class lookup")) {
        std::cout << "[ESP] Required classes could not be resolved." << std::endl;
        std::scoped_lock lock(stateMutex_);
        SetLookupDebugState(debugState_, "Required ESP classes missing", lookupFailures);
        return;
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
    if (playerClass != nullptr) {
        getHealthMethodID = env->GetMethodID(playerClass, mc_mappings::player::GetHealth.name,
                                             mc_mappings::player::GetHealth.signature);
    }
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
    if (playerClass != nullptr && getHealthMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "Player", mc_mappings::player::GetHealth.name,
                            mc_mappings::player::GetHealth.signature);
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
        oldXFieldID == nullptr || oldYFieldID == nullptr || oldZFieldID == nullptr ||
        ClearPendingJniException(env, "required ESP methods")) {
        std::cout << "[ESP] Required level/entity methods could not be resolved." << std::endl;
        std::scoped_lock lock(stateMutex_);
        SetLookupDebugState(debugState_, "Required ESP methods missing", lookupFailures);
        return;
    }

    deltaTrackerField = env->GetFieldID(Minecraft::getMinecraftClass(), mc_mappings::minecraft::DeltaTracker.name,
                                        mc_mappings::minecraft::DeltaTracker.signature);
    getGameTimeDeltaPartialTickMethodID = env->GetMethodID(
        deltaTrackerTimerClass,
        mc_mappings::delta_tracker_timer::GetGameTimeDeltaPartialTick.name,
        mc_mappings::delta_tracker_timer::GetGameTimeDeltaPartialTick.signature);
    gameRendererField = env->GetFieldID(Minecraft::getMinecraftClass(), mc_mappings::minecraft::GameRenderer.name,
                                        mc_mappings::minecraft::GameRenderer.signature);
    getMainCameraMethodID = env->GetMethodID(gameRendererClass, mc_mappings::game_renderer::GetMainCamera.name,
                                             mc_mappings::game_renderer::GetMainCamera.signature);
    getRenderFovMethodID = env->GetMethodID(gameRendererClass, mc_mappings::game_renderer::GetFov.name,
                                            mc_mappings::game_renderer::GetFov.signature);
    getCameraPositionMethodID = env->GetMethodID(cameraClass, mc_mappings::camera::GetPosition.name,
                                                 mc_mappings::camera::GetPosition.signature);
    getCameraYawMethodID = env->GetMethodID(cameraClass, mc_mappings::camera::GetYRot.name,
                                            mc_mappings::camera::GetYRot.signature);
    getCameraPitchMethodID = env->GetMethodID(cameraClass, mc_mappings::camera::GetXRot.name,
                                              mc_mappings::camera::GetXRot.signature);
    vec3XFieldID = env->GetFieldID(vec3Class, mc_mappings::vec3::X.name, mc_mappings::vec3::X.signature);
    vec3YFieldID = env->GetFieldID(vec3Class, mc_mappings::vec3::Y.name, mc_mappings::vec3::Y.signature);
    vec3ZFieldID = env->GetFieldID(vec3Class, mc_mappings::vec3::Z.name, mc_mappings::vec3::Z.signature);

    if (deltaTrackerField == nullptr) {
        AppendMemberFailure(lookupFailures, "field", "Minecraft", mc_mappings::minecraft::DeltaTracker.name,
                            mc_mappings::minecraft::DeltaTracker.signature);
    }
    if (getGameTimeDeltaPartialTickMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "DeltaTracker.Timer",
                            mc_mappings::delta_tracker_timer::GetGameTimeDeltaPartialTick.name,
                            mc_mappings::delta_tracker_timer::GetGameTimeDeltaPartialTick.signature);
    }
    if (gameRendererField == nullptr) {
        AppendMemberFailure(lookupFailures, "field", "Minecraft", mc_mappings::minecraft::GameRenderer.name,
                            mc_mappings::minecraft::GameRenderer.signature);
    }
    if (getMainCameraMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "GameRenderer", mc_mappings::game_renderer::GetMainCamera.name,
                            mc_mappings::game_renderer::GetMainCamera.signature);
    }
    if (getRenderFovMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "GameRenderer", mc_mappings::game_renderer::GetFov.name,
                            mc_mappings::game_renderer::GetFov.signature);
    }
    if (getCameraPositionMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "Camera", mc_mappings::camera::GetPosition.name,
                            mc_mappings::camera::GetPosition.signature);
    }
    if (getCameraYawMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "Camera", mc_mappings::camera::GetYRot.name,
                            mc_mappings::camera::GetYRot.signature);
    }
    if (getCameraPitchMethodID == nullptr) {
        AppendMemberFailure(lookupFailures, "method", "Camera", mc_mappings::camera::GetXRot.name,
                            mc_mappings::camera::GetXRot.signature);
    }
    if (vec3XFieldID == nullptr) {
        AppendMemberFailure(lookupFailures, "field", "Vec3", mc_mappings::vec3::X.name, mc_mappings::vec3::X.signature);
    }
    if (vec3YFieldID == nullptr) {
        AppendMemberFailure(lookupFailures, "field", "Vec3", mc_mappings::vec3::Y.name, mc_mappings::vec3::Y.signature);
    }
    if (vec3ZFieldID == nullptr) {
        AppendMemberFailure(lookupFailures, "field", "Vec3", mc_mappings::vec3::Z.name, mc_mappings::vec3::Z.signature);
    }

    if (deltaTrackerField == nullptr || getGameTimeDeltaPartialTickMethodID == nullptr ||
        gameRendererField == nullptr || getMainCameraMethodID == nullptr || getRenderFovMethodID == nullptr ||
        getCameraPositionMethodID == nullptr || getCameraYawMethodID == nullptr || getCameraPitchMethodID == nullptr ||
        vec3XFieldID == nullptr || vec3YFieldID == nullptr || vec3ZFieldID == nullptr ||
        ClearPendingJniException(env, "required render camera methods")) {
        std::cout << "[ESP] Required render camera methods could not be resolved." << std::endl;
        std::scoped_lock lock(stateMutex_);
        SetLookupDebugState(debugState_, "Required render camera methods missing", lookupFailures);
        return;
    }

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

bool Esp::HasRenderCameraLookups() const {
    return gameRendererField != nullptr &&
           getMainCameraMethodID != nullptr &&
           getCameraPositionMethodID != nullptr &&
           getCameraYawMethodID != nullptr &&
           getRenderFovMethodID != nullptr &&
           getCameraPitchMethodID != nullptr &&
           vec3XFieldID != nullptr &&
           vec3YFieldID != nullptr &&
           vec3ZFieldID != nullptr;
}

bool Esp::TryReadRenderCameraState(JNIEnv *env, CameraState &renderCamera) const {
    if (env == nullptr || !HasRenderCameraLookups()) {
        return false;
    }

    jobject gameRendererObj = env->GetObjectField(Minecraft::getMcInstance(), gameRendererField);
    if (gameRendererObj == nullptr) {
        return false;
    }

    jobject cameraObj = env->CallObjectMethod(gameRendererObj, getMainCameraMethodID);
    if (cameraObj == nullptr || ClearPendingJniException(env, "GameRenderer.getMainCamera")) {
        if (cameraObj != nullptr) {
            env->DeleteLocalRef(cameraObj);
        }
        env->DeleteLocalRef(gameRendererObj);
        return false;
    }

    jobject positionObj = env->CallObjectMethod(cameraObj, getCameraPositionMethodID);
    if (positionObj == nullptr || ClearPendingJniException(env, "Camera.getPosition")) {
        if (positionObj != nullptr) {
            env->DeleteLocalRef(positionObj);
        }
        env->DeleteLocalRef(cameraObj);
        env->DeleteLocalRef(gameRendererObj);
        return false;
    }

    const double cameraX = env->GetDoubleField(positionObj, vec3XFieldID);
    const double cameraY = env->GetDoubleField(positionObj, vec3YFieldID);
    const double cameraZ = env->GetDoubleField(positionObj, vec3ZFieldID);
    const float cameraYaw = env->CallFloatMethod(cameraObj, getCameraYawMethodID);
    const float cameraPitch = env->CallFloatMethod(cameraObj, getCameraPitchMethodID);
    const float frameTime = ReadCurrentFrameTime(env);
    const jfloat renderFovValue = env->CallFloatMethod(gameRendererObj, getRenderFovMethodID, cameraObj, frameTime,
                                                       JNI_TRUE);

    const bool readFailed = ClearPendingJniException(env, "render camera reads");
    env->DeleteLocalRef(positionObj);
    env->DeleteLocalRef(cameraObj);
    env->DeleteLocalRef(gameRendererObj);
    if (readFailed) {
        return false;
    }

    if (!std::isfinite(renderFovValue) || renderFovValue <= 1.0f) {
        return false;
    }

    renderCamera = {};
    renderCamera.previousX = cameraX;
    renderCamera.previousY = cameraY;
    renderCamera.previousZ = cameraZ;
    renderCamera.x = cameraX;
    renderCamera.y = cameraY;
    renderCamera.z = cameraZ;
    renderCamera.eyeHeightOffset = 0.0;
    renderCamera.yawDegrees = NormalizeDegrees(cameraYaw);
    renderCamera.pitchDegrees = NormalizeDegrees(cameraPitch);
    renderCamera.fovDegrees = static_cast<double>(renderFovValue);
    renderCamera.valid = IsFinite(renderCamera.x) && IsFinite(renderCamera.y) && IsFinite(renderCamera.z) &&
                         IsFinite(renderCamera.yawDegrees) && IsFinite(renderCamera.pitchDegrees) &&
                         IsFinite(renderCamera.fovDegrees);
    return renderCamera.valid;
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
    bool renderCameraAvailable = HasRenderCameraLookups();
    const bool renderCameraUsed = TryReadRenderCameraState(env, updatedCamera);

    jobject localPlayer = LocalPlayer::getLocalPlayerObject();
    const bool localPlayerValid = localPlayer != nullptr;
    bool levelValid = false;
    bool playerListValid = false;
    int playerCountValue = 0;

    jobject levelObj = env->GetObjectField(Minecraft::getMcInstance(), levelField);
    if (levelObj == nullptr || ClearPendingJniException(env, "Minecraft.level read")) {
        std::scoped_lock lock(stateMutex_);
        targets_.clear();
        cameraState_ = updatedCamera;
        debugState_.localPlayerValid = localPlayerValid;
        debugState_.levelValid = false;
        debugState_.playerListValid = false;
        debugState_.cameraValid = updatedCamera.valid;
        debugState_.renderCameraAvailable = renderCameraAvailable;
        debugState_.renderCameraUsed = renderCameraUsed;
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
        debugState_.renderCameraAvailable = renderCameraAvailable;
        debugState_.renderCameraUsed = renderCameraUsed;
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
            -1.0f,
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
        if (getHealthMethodID != nullptr) {
            target.health = env->CallFloatMethod(playerObj, getHealthMethodID);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                target.health = -1.0f;
            }
        }

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
    debugState_.renderCameraAvailable = renderCameraAvailable;
    debugState_.renderCameraUsed = renderCameraUsed;
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

void Esp::RenderOverlay(bool drawTracer, bool drawBox, const float *tracerColor, float tracerThickness,
                        const float *boxColor, float boxThickness) const {
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
    const ImVec2 tracerStart(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 42.0f);
    const ImU32 tracerDrawColor = ColorFromArray(tracerColor, IM_COL32(255, 70, 70, 220));
    const ImU32 boxDrawColor = ColorFromArray(boxColor, IM_COL32(255, 70, 70, 210));
    const ImU32 healthTextColor = IM_COL32(150, 255, 150, 235);
    const ImU32 distanceTextColor = IM_COL32(245, 248, 255, 235);
    const float tracerLineThickness = std::clamp(tracerThickness, 0.5f, 8.0f);
    const float boxLineThickness = std::clamp(boxThickness, 0.5f, 8.0f);
    const float tracerPointRadius = std::max(2.0f, tracerLineThickness + 1.1f);
    ImVec2 textPosition(15.0f, 15.0f);
    const ImU32 textColor = IM_COL32(255, 255, 255, 230);
    char lineBuffer[256]{};

    std::snprintf(lineBuffer, sizeof(lineBuffer), "ESP init=%d local=%d level=%d list=%d cam=%d rcam=%d/%d",
                  debugSnapshot.initialized ? 1 : 0,
                  debugSnapshot.localPlayerValid ? 1 : 0,
                  debugSnapshot.levelValid ? 1 : 0,
                  debugSnapshot.playerListValid ? 1 : 0,
                  debugSnapshot.cameraValid ? 1 : 0,
                  debugSnapshot.renderCameraAvailable ? 1 : 0,
                  debugSnapshot.renderCameraUsed ? 1 : 0);
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
        const double interpolatedBaseY = Lerp(target.previousY, target.y, frameTime);
        const double interpolatedY = interpolatedBaseY + target.eyeHeightOffset;
        const double interpolatedZ = Lerp(target.previousZ, target.z, frameTime);
        ImVec2 screenPosition{};
        if (!WorldToScreen({interpolatedX, interpolatedY - 0.60, interpolatedZ}, cameraSnapshot, io.DisplaySize.x, io.DisplaySize.y,
                           screenPosition)) {
            continue;
        }

        if (!IsFinite(screenPosition.x) || !IsFinite(screenPosition.y)) {
            continue;
        }

        ImVec2 headScreenPosition{};
        ImVec2 feetScreenPosition{};
        ImVec2 boxCenterScreenPosition{};
        const double headWorldY = interpolatedBaseY + target.eyeHeightOffset + 0.18;
        const double feetWorldY = interpolatedBaseY - 0.08;
        const double boxCenterWorldY = interpolatedBaseY + target.eyeHeightOffset * 0.45;
        const bool headVisible = WorldToScreen({interpolatedX, headWorldY, interpolatedZ}, cameraSnapshot, io.DisplaySize.x,
                                               io.DisplaySize.y, headScreenPosition, false);
        const bool feetVisible = WorldToScreen({interpolatedX, feetWorldY, interpolatedZ}, cameraSnapshot, io.DisplaySize.x,
                                               io.DisplaySize.y, feetScreenPosition, false);
        const bool boxCenterVisible = WorldToScreen({interpolatedX, boxCenterWorldY, interpolatedZ}, cameraSnapshot,
                                                    io.DisplaySize.x, io.DisplaySize.y, boxCenterScreenPosition, false);

        if (drawBox && headVisible && feetVisible && boxCenterVisible) {
            const float top = std::min(headScreenPosition.y, feetScreenPosition.y);
            const float bottom = std::max(headScreenPosition.y, feetScreenPosition.y);
            const float height = bottom - top;
            if (height >= 6.0f) {
                const float centerX = boxCenterScreenPosition.x;
                const float halfWidth = height * 0.19f;
                drawList->AddRect(ImVec2(centerX - halfWidth, top), ImVec2(centerX + halfWidth, bottom),
                                  boxDrawColor, 0.0f, 0, boxLineThickness);

                if (target.health >= 0.0f && std::isfinite(target.health)) {
                    char healthBuffer[32]{};
                    std::snprintf(healthBuffer, sizeof(healthBuffer), "%.1f HP", target.health);
                    const ImVec2 healthTextSize = ImGui::CalcTextSize(healthBuffer);
                    const ImVec2 healthTextPosition(centerX - healthTextSize.x * 0.5f, top - healthTextSize.y - 3.0f);
                    drawList->AddText(healthTextPosition, healthTextColor, healthBuffer);
                }

                const double distanceX = interpolatedX - cameraSnapshot.x;
                const double distanceY = interpolatedBaseY - cameraSnapshot.y;
                const double distanceZ = interpolatedZ - cameraSnapshot.z;
                const double distanceMeters = std::sqrt(distanceX * distanceX + distanceY * distanceY + distanceZ * distanceZ);
                if (IsFinite(distanceMeters)) {
                    char distanceBuffer[32]{};
                    std::snprintf(distanceBuffer, sizeof(distanceBuffer), "[%.0fm]", distanceMeters);
                    const ImVec2 distanceTextSize = ImGui::CalcTextSize(distanceBuffer);
                    const ImVec2 distanceTextPosition(centerX - distanceTextSize.x * 0.5f, bottom + 4.0f);
                    drawList->AddText(distanceTextPosition, distanceTextColor, distanceBuffer);
                }
            }
        }

        if (drawTracer) {
            drawList->AddLine(tracerStart, screenPosition, tracerDrawColor, tracerLineThickness);
            drawList->AddCircleFilled(screenPosition, tracerPointRadius, tracerDrawColor);
        }
    }
}
