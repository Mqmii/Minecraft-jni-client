#pragma once

#include <jni.h>

#include <mutex>
#include <string>
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

    mutable std::mutex stateMutex_;
    std::vector<Target> targets_;
    CameraState cameraState_{};
    DebugState debugState_{};
};
