#pragma once

#include <jni.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../classes/minecraft.hpp"

class Esp {
public:
    struct Target {
        double previousX{};
        double previousY{};
        double previousZ{};
        double x{};
        double y{};
        double z{};
        double eyeHeightOffset{};
        float health{-1.0f};
        std::string name{};
    };

    struct CameraState {
        double previousX{};
        double previousY{};
        double previousZ{};
        double x{};
        double y{};
        double z{};
        double eyeHeightOffset{};
        double yawDegrees{};
        double pitchDegrees{};
        double fovDegrees{90.0};
        bool valid{false};
    };

    struct DebugState {
        bool initialized{false};
        bool localPlayerValid{false};
        bool levelValid{false};
        bool playerListValid{false};
        bool cameraValid{false};
        bool renderCameraAvailable{false};
        bool renderCameraUsed{false};
        int playerCount{0};
        int targetCount{0};
        double cameraX{};
        double cameraY{};
        double cameraZ{};
        double yawDegrees{};
        double pitchDegrees{};
        double fovDegrees{90.0};
        std::string lastStatus{"ESP not initialized"};
        std::string lookupDetails{};
    };

    struct NameCacheEntry {
        int entityId{};
        std::string name{};
        uint64_t lastRefreshTick{};
        uint64_t lastSeenTick{};
    };

    jclass levelClass{};
    jclass listClass{};
    jclass deltaTrackerTimerClass{};
    jclass gameRendererClass{};
    jclass cameraClass{};
    jclass vec3Class{};
    jclass playerClass{};

    jfieldID levelField{};
    jfieldID deltaTrackerField{};
    jfieldID gameRendererField{};
    jfieldID oldXFieldID{};
    jfieldID oldYFieldID{};
    jfieldID oldZFieldID{};
    jfieldID vec3XFieldID{};
    jfieldID vec3YFieldID{};
    jfieldID vec3ZFieldID{};

    jmethodID getPlayersMethodID{};
    jmethodID listSizeMethodID{};
    jmethodID listGetMethodID{};
    jmethodID getXMethodID{};
    jmethodID getYMethodID{};
    jmethodID getEyeYMethodID{};
    jmethodID getZMethodID{};
    jmethodID getGameTimeDeltaPartialTickMethodID{};
    jmethodID getHealthMethodID{};
    jmethodID getEntityIdMethodID{};
    jmethodID getNameMethodID{};
    jmethodID getStringMethodID{};
    jmethodID getMainCameraMethodID{};
    jmethodID getCameraPositionMethodID{};
    jmethodID getCameraYawMethodID{};
    jmethodID getCameraPitchMethodID{};
    jmethodID getRenderFovMethodID{};

    Esp();
    ~Esp();
    bool IsInitialized() const;
    void Tick();
    void RenderOverlay(bool drawTracer, bool drawBox, bool showDebug, const float *tracerColor, float tracerThickness,
                       const float *boxColor, float boxThickness) const;

private:
    void SetStatusLocked(const char *status);
    void SetStatus(const char *status);
    bool initialized_{false};
    float ReadCurrentFrameTime(JNIEnv *env) const;
    bool TryReadRenderCameraState(JNIEnv *env, CameraState &renderCamera) const;
    bool HasRenderCameraLookups() const;
    std::string ResolveCachedPlayerName(JNIEnv *env, jobject playerObj, int entityId, uint64_t currentTick);
    std::string QueryPlayerName(JNIEnv *env, jobject playerObj) const;
    void PruneNameCache(JNIEnv *env, uint64_t currentTick);
    void ClearNameCache(JNIEnv *env);

    mutable std::mutex stateMutex_;
    std::vector<Target> targets_;
    std::unordered_map<int, NameCacheEntry> nameCache_;
    CameraState cameraState_{};
    DebugState debugState_{};
};
