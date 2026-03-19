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
    jclass optionsClass{};
    jclass optionInstanceClass{};
    jclass numberClass{};

    jfieldID levelField{};
    jfieldID deltaTrackerField{};
    jfieldID optionsField{};
    jfieldID fovField{};
    jfieldID optionValueField{};
    jfieldID oldXFieldID{};
    jfieldID oldYFieldID{};
    jfieldID oldZFieldID{};

    jmethodID getPlayersMethodID{};
    jmethodID listSizeMethodID{};
    jmethodID listGetMethodID{};
    jmethodID getXMethodID{};
    jmethodID getYMethodID{};
    jmethodID getEyeYMethodID{};
    jmethodID getZMethodID{};
    jmethodID getYawMethodID{};
    jmethodID getPitchMethodID{};
    jmethodID getGameTimeDeltaPartialTickMethodID{};
    jmethodID numberDoubleValueMethodID{};

    Esp();
    bool IsInitialized() const;
    void Tick();
    void RenderOverlay() const;

private:
    void SetStatusLocked(const char *status);
    void SetStatus(const char *status);
    bool initialized_{false};
    double ReadCurrentFov(JNIEnv *env) const;
    float ReadCurrentFrameTime(JNIEnv *env) const;

    mutable std::mutex stateMutex_;
    std::vector<Target> targets_;
    CameraState cameraState_{};
    DebugState debugState_{};
};
