#pragma once

#include <cstdint>
#include <unordered_map>
#include <windows.h>

#include "../modules/Esp.hpp"

class EspOverlayRenderer {
public:
    bool Render(HGLRC currentContext, const Esp::RenderSnapshot &snapshot, const Esp::RenderCameraState &cameraState,
                bool drawTracer, bool drawBox, const float *tracerColor, float tracerThickness,
                const float *boxColor, float boxThickness);
    void ShutdownForCurrentContext(HGLRC currentContext);
    void AbandonContext();

private:
    struct SmoothedTargetState {
        double x{};
        double y{};
        double z{};
        bool initialized{false};
        double lastUpdateSeconds{};
        double lastSeenSeconds{};
    };

    bool EnsureInitialized(HGLRC currentContext);
    void DestroyResources();

    HGLRC renderContext_ = nullptr;
    unsigned int program_ = 0;
    unsigned int vertexArray_ = 0;
    unsigned int vertexBuffer_ = 0;
    std::unordered_map<int, SmoothedTargetState> smoothedTargets_{};
    bool glFunctionsLoaded_ = false;
    bool loggedInitializationFailure_ = false;
};
