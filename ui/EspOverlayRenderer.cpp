#include "EspOverlayRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/trigonometric.hpp>

namespace {
struct GlFunctions {
    PFNGLACTIVETEXTUREPROC activeTexture{};
    PFNGLATTACHSHADERPROC attachShader{};
    PFNGLBINDATTRIBLOCATIONPROC bindAttribLocation{};
    PFNGLBINDBUFFERPROC bindBuffer{};
    PFNGLBINDVERTEXARRAYPROC bindVertexArray{};
    PFNGLBLENDEQUATIONSEPARATEPROC blendEquationSeparate{};
    PFNGLBLENDFUNCSEPARATEPROC blendFuncSeparate{};
    PFNGLBUFFERDATAPROC bufferData{};
    PFNGLCOMPILESHADERPROC compileShader{};
    PFNGLCREATEPROGRAMPROC createProgram{};
    PFNGLCREATESHADERPROC createShader{};
    PFNGLDELETEBUFFERSPROC deleteBuffers{};
    PFNGLDELETEPROGRAMPROC deleteProgram{};
    PFNGLDELETESHADERPROC deleteShader{};
    PFNGLDELETEVERTEXARRAYSPROC deleteVertexArrays{};
    PFNGLDETACHSHADERPROC detachShader{};
    PFNGLENABLEVERTEXATTRIBARRAYPROC enableVertexAttribArray{};
    PFNGLGENBUFFERSPROC genBuffers{};
    PFNGLGENVERTEXARRAYSPROC genVertexArrays{};
    PFNGLGETPROGRAMINFOLOGPROC getProgramInfoLog{};
    PFNGLGETPROGRAMIVPROC getProgramiv{};
    PFNGLGETSHADERINFOLOGPROC getShaderInfoLog{};
    PFNGLGETSHADERIVPROC getShaderiv{};
    PFNGLLINKPROGRAMPROC linkProgram{};
    PFNGLSHADERSOURCEPROC shaderSource{};
    PFNGLUSEPROGRAMPROC useProgram{};
    PFNGLVERTEXATTRIBPOINTERPROC vertexAttribPointer{};
};

GlFunctions g_gl{};
bool g_glLoaded = false;

template <typename T>
T LoadGlProc(const char *name) {
    PROC proc = wglGetProcAddress(name);
    if (proc == nullptr || proc == reinterpret_cast<PROC>(0x1) || proc == reinterpret_cast<PROC>(0x2) ||
        proc == reinterpret_cast<PROC>(0x3) || proc == reinterpret_cast<PROC>(-1)) {
        static HMODULE openglModule = LoadLibraryA("opengl32.dll");
        proc = openglModule != nullptr ? GetProcAddress(openglModule, name) : nullptr;
    }
    return reinterpret_cast<T>(proc);
}

bool LoadGlFunctions() {
    if (g_glLoaded) {
        return true;
    }

    g_gl.activeTexture = LoadGlProc<PFNGLACTIVETEXTUREPROC>("glActiveTexture");
    g_gl.attachShader = LoadGlProc<PFNGLATTACHSHADERPROC>("glAttachShader");
    g_gl.bindAttribLocation = LoadGlProc<PFNGLBINDATTRIBLOCATIONPROC>("glBindAttribLocation");
    g_gl.bindBuffer = LoadGlProc<PFNGLBINDBUFFERPROC>("glBindBuffer");
    g_gl.bindVertexArray = LoadGlProc<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray");
    g_gl.blendEquationSeparate = LoadGlProc<PFNGLBLENDEQUATIONSEPARATEPROC>("glBlendEquationSeparate");
    g_gl.blendFuncSeparate = LoadGlProc<PFNGLBLENDFUNCSEPARATEPROC>("glBlendFuncSeparate");
    g_gl.bufferData = LoadGlProc<PFNGLBUFFERDATAPROC>("glBufferData");
    g_gl.compileShader = LoadGlProc<PFNGLCOMPILESHADERPROC>("glCompileShader");
    g_gl.createProgram = LoadGlProc<PFNGLCREATEPROGRAMPROC>("glCreateProgram");
    g_gl.createShader = LoadGlProc<PFNGLCREATESHADERPROC>("glCreateShader");
    g_gl.deleteBuffers = LoadGlProc<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers");
    g_gl.deleteProgram = LoadGlProc<PFNGLDELETEPROGRAMPROC>("glDeleteProgram");
    g_gl.deleteShader = LoadGlProc<PFNGLDELETESHADERPROC>("glDeleteShader");
    g_gl.deleteVertexArrays = LoadGlProc<PFNGLDELETEVERTEXARRAYSPROC>("glDeleteVertexArrays");
    g_gl.detachShader = LoadGlProc<PFNGLDETACHSHADERPROC>("glDetachShader");
    g_gl.enableVertexAttribArray = LoadGlProc<PFNGLENABLEVERTEXATTRIBARRAYPROC>("glEnableVertexAttribArray");
    g_gl.genBuffers = LoadGlProc<PFNGLGENBUFFERSPROC>("glGenBuffers");
    g_gl.genVertexArrays = LoadGlProc<PFNGLGENVERTEXARRAYSPROC>("glGenVertexArrays");
    g_gl.getProgramInfoLog = LoadGlProc<PFNGLGETPROGRAMINFOLOGPROC>("glGetProgramInfoLog");
    g_gl.getProgramiv = LoadGlProc<PFNGLGETPROGRAMIVPROC>("glGetProgramiv");
    g_gl.getShaderInfoLog = LoadGlProc<PFNGLGETSHADERINFOLOGPROC>("glGetShaderInfoLog");
    g_gl.getShaderiv = LoadGlProc<PFNGLGETSHADERIVPROC>("glGetShaderiv");
    g_gl.linkProgram = LoadGlProc<PFNGLLINKPROGRAMPROC>("glLinkProgram");
    g_gl.shaderSource = LoadGlProc<PFNGLSHADERSOURCEPROC>("glShaderSource");
    g_gl.useProgram = LoadGlProc<PFNGLUSEPROGRAMPROC>("glUseProgram");
    g_gl.vertexAttribPointer = LoadGlProc<PFNGLVERTEXATTRIBPOINTERPROC>("glVertexAttribPointer");

    g_glLoaded = g_gl.activeTexture != nullptr &&
                 g_gl.attachShader != nullptr &&
                 g_gl.bindAttribLocation != nullptr &&
                 g_gl.bindBuffer != nullptr &&
                 g_gl.bindVertexArray != nullptr &&
                 g_gl.blendEquationSeparate != nullptr &&
                 g_gl.blendFuncSeparate != nullptr &&
                 g_gl.bufferData != nullptr &&
                 g_gl.compileShader != nullptr &&
                 g_gl.createProgram != nullptr &&
                 g_gl.createShader != nullptr &&
                 g_gl.deleteBuffers != nullptr &&
                 g_gl.deleteProgram != nullptr &&
                 g_gl.deleteShader != nullptr &&
                 g_gl.deleteVertexArrays != nullptr &&
                 g_gl.detachShader != nullptr &&
                 g_gl.enableVertexAttribArray != nullptr &&
                 g_gl.genBuffers != nullptr &&
                 g_gl.genVertexArrays != nullptr &&
                 g_gl.getProgramInfoLog != nullptr &&
                 g_gl.getProgramiv != nullptr &&
                 g_gl.getShaderInfoLog != nullptr &&
                 g_gl.getShaderiv != nullptr &&
                 g_gl.linkProgram != nullptr &&
                 g_gl.shaderSource != nullptr &&
                 g_gl.useProgram != nullptr &&
                 g_gl.vertexAttribPointer != nullptr;
    return g_glLoaded;
}

bool IsFinite(double value) {
    return std::isfinite(value) != 0;
}

double Lerp(double start, double end, float alpha) {
    return start + (end - start) * static_cast<double>(alpha);
}

double GetHighResolutionTimeSeconds() {
    static const LARGE_INTEGER frequency = []() {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return value;
    }();

    if (frequency.QuadPart <= 0) {
        return 0.0;
    }

    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
}

double ComputeSmoothingAlpha(double deltaSeconds, double trackingError) {
    constexpr double kBaseSmoothingStrength = 22.0;
    constexpr double kMaxBoostStrength = 140.0;
    const double clampedDelta = std::clamp(deltaSeconds, 0.0, 0.100);
    const double clampedError = std::clamp(trackingError, 0.0, 3.5);
    const double adaptiveStrength = kBaseSmoothingStrength + (clampedError * 40.0);
    const double effectiveStrength = std::clamp(adaptiveStrength, kBaseSmoothingStrength, kMaxBoostStrength);
    return std::clamp(1.0 - std::exp(-effectiveStrength * clampedDelta), 0.0, 1.0);
}

struct Vec2f {
    float x{};
    float y{};
};

struct Vec3 {
    double x{};
    double y{};
    double z{};
};

struct Color4f {
    float r{};
    float g{};
    float b{};
    float a{};
};

struct OverlayVertex {
    float x{};
    float y{};
    float r{};
    float g{};
    float b{};
    float a{};
};

double NormalizeDegrees(double degrees) {
    double normalized = std::fmod(degrees, 360.0);
    if (normalized > 180.0) {
        normalized -= 360.0;
    } else if (normalized < -180.0) {
        normalized += 360.0;
    }
    return normalized;
}

Color4f ClampColor(const float *color, const Color4f &fallback) {
    if (color == nullptr) {
        return fallback;
    }

    return {
        std::clamp(color[0], 0.0f, 1.0f),
        std::clamp(color[1], 0.0f, 1.0f),
        std::clamp(color[2], 0.0f, 1.0f),
        std::clamp(color[3], 0.0f, 1.0f),
    };
}

Vec2f ToClipSpace(const Vec2f &position, float width, float height) {
    return {
        (position.x / width) * 2.0f - 1.0f,
        1.0f - (position.y / height) * 2.0f,
    };
}

void PushTriangle(std::vector<OverlayVertex> &vertices, const Vec2f &a, const Vec2f &b, const Vec2f &c,
                  float width, float height, const Color4f &color) {
    const Vec2f clipA = ToClipSpace(a, width, height);
    const Vec2f clipB = ToClipSpace(b, width, height);
    const Vec2f clipC = ToClipSpace(c, width, height);
    vertices.push_back({clipA.x, clipA.y, color.r, color.g, color.b, color.a});
    vertices.push_back({clipB.x, clipB.y, color.r, color.g, color.b, color.a});
    vertices.push_back({clipC.x, clipC.y, color.r, color.g, color.b, color.a});
}

void PushQuad(std::vector<OverlayVertex> &vertices, const Vec2f &topLeft, const Vec2f &topRight,
              const Vec2f &bottomRight, const Vec2f &bottomLeft, float width, float height, const Color4f &color) {
    PushTriangle(vertices, topLeft, topRight, bottomRight, width, height, color);
    PushTriangle(vertices, topLeft, bottomRight, bottomLeft, width, height, color);
}

void PushThickLine(std::vector<OverlayVertex> &vertices, const Vec2f &start, const Vec2f &end, float thickness,
                   float width, float height, const Color4f &color) {
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.001f) {
        return;
    }

    const float halfThickness = std::max(0.5f, thickness * 0.5f);
    const Vec2f offset{-dy / length * halfThickness, dx / length * halfThickness};
    PushQuad(vertices,
             {start.x - offset.x, start.y - offset.y},
             {start.x + offset.x, start.y + offset.y},
             {end.x + offset.x, end.y + offset.y},
             {end.x - offset.x, end.y - offset.y},
             width, height, color);
}

void PushPointQuad(std::vector<OverlayVertex> &vertices, const Vec2f &center, float radius, float width, float height,
                   const Color4f &color) {
    const float size = std::max(1.0f, radius);
    PushQuad(vertices,
             {center.x - size, center.y - size},
             {center.x + size, center.y - size},
             {center.x + size, center.y + size},
             {center.x - size, center.y + size},
             width, height, color);
}

bool ProjectToNdc(const Vec3 &worldPosition, const Esp::CameraState &cameraState, float width, float height,
                  double &normalizedX, double &normalizedY, bool &inFront) {
    if (!cameraState.valid || width <= 0.0f || height <= 0.0f) {
        return false;
    }

    if (!IsFinite(worldPosition.x) || !IsFinite(worldPosition.y) || !IsFinite(worldPosition.z) ||
        !IsFinite(cameraState.x) || !IsFinite(cameraState.y) || !IsFinite(cameraState.z) ||
        !IsFinite(cameraState.yawDegrees) || !IsFinite(cameraState.pitchDegrees) || !IsFinite(cameraState.fovDegrees)) {
        return false;
    }

    const float aspectRatio = width / height;
    const float clampedFov = std::clamp(static_cast<float>(cameraState.fovDegrees), 30.0f, 170.0f);
    const float yawRadians = glm::radians(static_cast<float>(NormalizeDegrees(cameraState.yawDegrees)));
    const float pitchRadians = glm::radians(static_cast<float>(NormalizeDegrees(-cameraState.pitchDegrees)));

    glm::vec3 eye(static_cast<float>(cameraState.x), static_cast<float>(cameraState.y), static_cast<float>(cameraState.z));
    glm::vec3 forward(
        -std::sin(yawRadians) * std::cos(pitchRadians),
        std::sin(pitchRadians),
        std::cos(yawRadians) * std::cos(pitchRadians));
    if (!std::isfinite(forward.x) || !std::isfinite(forward.y) || !std::isfinite(forward.z) ||
        glm::length(forward) <= 0.0001f) {
        return false;
    }

    const glm::mat4 view = glm::lookAtRH(eye, eye + glm::normalize(forward), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::perspectiveRH_NO(glm::radians(clampedFov), aspectRatio, 0.05f, 4096.0f);
    const glm::vec4 clip = projection * view * glm::vec4(
                               static_cast<float>(worldPosition.x),
                               static_cast<float>(worldPosition.y),
                               static_cast<float>(worldPosition.z),
                               1.0f);
    if (!std::isfinite(clip.x) || !std::isfinite(clip.y) || !std::isfinite(clip.z) || !std::isfinite(clip.w) ||
        std::abs(clip.w) <= 0.0001f) {
        return false;
    }

    inFront = clip.w > 0.0f;
    const float inverseW = 1.0f / std::abs(clip.w);
    normalizedX = static_cast<double>((inFront ? clip.x : -clip.x) * inverseW);
    normalizedY = static_cast<double>((inFront ? clip.y : -clip.y) * inverseW);
    return IsFinite(normalizedX) && IsFinite(normalizedY);
}

bool ComputeOffscreenIndicatorNormalized(const Vec3 &worldPosition, const Esp::CameraState &cameraState, float width,
                                         float height, double &normalizedX, double &normalizedY) {
    if (!cameraState.valid || width <= 0.0f || height <= 0.0f) {
        return false;
    }

    if (!IsFinite(worldPosition.x) || !IsFinite(worldPosition.y) || !IsFinite(worldPosition.z) ||
        !IsFinite(cameraState.x) || !IsFinite(cameraState.y) || !IsFinite(cameraState.z) ||
        !IsFinite(cameraState.yawDegrees) || !IsFinite(cameraState.pitchDegrees) || !IsFinite(cameraState.fovDegrees)) {
        return false;
    }

    const float aspectRatio = width / height;
    const float yawRadians = glm::radians(static_cast<float>(NormalizeDegrees(cameraState.yawDegrees)));
    const float pitchRadians = glm::radians(static_cast<float>(NormalizeDegrees(-cameraState.pitchDegrees)));
    glm::vec3 eye(
        static_cast<float>(cameraState.x),
        static_cast<float>(cameraState.y),
        static_cast<float>(cameraState.z));
    glm::vec3 forward(
        -std::sin(yawRadians) * std::cos(pitchRadians),
        std::sin(pitchRadians),
        std::cos(yawRadians) * std::cos(pitchRadians));
    if (!std::isfinite(forward.x) || !std::isfinite(forward.y) || !std::isfinite(forward.z) ||
        glm::length(forward) <= 0.0001f) {
        return false;
    }

    const glm::mat4 view = glm::lookAtRH(eye, eye + glm::normalize(forward), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec4 viewSpace = view * glm::vec4(
                                    static_cast<float>(worldPosition.x),
                                    static_cast<float>(worldPosition.y),
                                    static_cast<float>(worldPosition.z),
                                    1.0f);
    if (!std::isfinite(viewSpace.x) || !std::isfinite(viewSpace.y) || !std::isfinite(viewSpace.z)) {
        return false;
    }

    double directionX = static_cast<double>(viewSpace.x) / static_cast<double>(aspectRatio);
    double directionY = static_cast<double>(viewSpace.y);
    if (viewSpace.z >= 0.0f) {
        directionY = -directionY;
    }

    if (std::abs(directionX) <= 0.000001 && std::abs(directionY) <= 0.000001) {
        directionY = -1.0;
    }

    const double scale = 1.0 / std::max(std::abs(directionX), std::abs(directionY));
    normalizedX = directionX * scale;
    normalizedY = directionY * scale;
    return IsFinite(normalizedX) && IsFinite(normalizedY);
}

bool WorldToScreen(const Vec3 &worldPosition, const Esp::CameraState &cameraState, float width, float height,
                   Vec2f &screenPosition, bool clampOffscreen = true) {
    double normalizedX{};
    double normalizedY{};
    bool inFront = false;
    if (!ProjectToNdc(worldPosition, cameraState, width, height, normalizedX, normalizedY, inFront)) {
        return false;
    }

    if (!clampOffscreen && !inFront) {
        return false;
    }

    const bool onScreen = inFront && std::abs(normalizedX) <= 1.0 && std::abs(normalizedY) <= 1.0;
    if (clampOffscreen && !onScreen) {
        if (!inFront &&
            !ComputeOffscreenIndicatorNormalized(worldPosition, cameraState, width, height, normalizedX, normalizedY)) {
            return false;
        }
    }

    const double absX = std::abs(normalizedX);
    const double absY = std::abs(normalizedY);
    double clampedX = normalizedX;
    double clampedY = normalizedY;

    if (!clampOffscreen && (absX > 1.0 || absY > 1.0)) {
        return false;
    }

    const bool offscreenIndicator = clampOffscreen && !onScreen;
    if (offscreenIndicator && (absX > 1.0 || absY > 1.0)) {
        const double scale = 1.0 / std::max(absX, absY);
        clampedX *= scale;
        clampedY *= scale;
    }

    if (offscreenIndicator) {
        const double trueHeightDelta = worldPosition.y - cameraState.y;
        const bool verticalPriority = std::abs(trueHeightDelta) >= 2.0;

        if (verticalPriority) {
            const double verticalSign = trueHeightDelta >= 0.0 ? 1.0 : -1.0;
            double horizontalSpread = clampedX * 2.4;
            if (std::abs(horizontalSpread) < 0.12 && std::abs(clampedX) > 0.02) {
                horizontalSpread = std::copysign(0.12, clampedX);
            }
            clampedX = std::clamp(horizontalSpread, -0.78, 0.78);
            clampedY = verticalSign;
        } else {
            const double horizontalStrength = std::abs(clampedX);
            const double sideSign = clampedX >= 0.0 ? 1.0 : -1.0;
            const double sideBiasSource = std::max(horizontalStrength, 0.0001);
            clampedY = std::clamp(clampedY / sideBiasSource, -0.72, 0.72);
            clampedX = sideSign;
        }
    }

    constexpr float screenPadding = 18.0f;
    screenPosition.x = static_cast<float>((clampedX + 1.0) * 0.5 * width);
    screenPosition.y = static_cast<float>((1.0 - clampedY) * 0.5 * height);
    screenPosition.x = std::clamp(screenPosition.x, screenPadding, width - screenPadding);
    screenPosition.y = std::clamp(screenPosition.y, screenPadding, height - screenPadding);
    return IsFinite(screenPosition.x) && IsFinite(screenPosition.y);
}

bool WorldToScreenVisible(const Vec3 &worldPosition, const Esp::CameraState &cameraState, float width, float height,
                          double &normalizedX, double &normalizedY) {
    bool inFront = false;
    return ProjectToNdc(worldPosition, cameraState, width, height, normalizedX, normalizedY, inFront) &&
           inFront &&
           std::abs(normalizedX) <= 1.0 &&
           std::abs(normalizedY) <= 1.0;
}

GLuint CompileShader(GLenum type, const char *source) {
    const GLuint shader = g_gl.createShader(type);
    if (shader == 0) {
        return 0;
    }

    g_gl.shaderSource(shader, 1, &source, nullptr);
    g_gl.compileShader(shader);

    GLint compiled = GL_FALSE;
    g_gl.getShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    char infoLog[512]{};
    GLsizei infoLength = 0;
    g_gl.getShaderInfoLog(shader, static_cast<GLsizei>(sizeof(infoLog)), &infoLength, infoLog);
    std::cout << "[ESP-GL] Shader compile failed: " << (infoLength > 0 ? infoLog : "<no info>") << std::endl;
    g_gl.deleteShader(shader);
    return 0;
}

struct GlStateGuard {
    GLint program = 0;
    GLint vertexArray = 0;
    GLint arrayBuffer = 0;
    GLint activeTexture = GL_TEXTURE0;
    GLint textureBinding2D = 0;
    GLint viewport[4]{};
    GLint blendSrcRgb = GL_ONE;
    GLint blendDstRgb = GL_ZERO;
    GLint blendSrcAlpha = GL_ONE;
    GLint blendDstAlpha = GL_ZERO;
    GLint blendEqRgb = GL_FUNC_ADD;
    GLint blendEqAlpha = GL_FUNC_ADD;
    GLboolean blendEnabled = GL_FALSE;
    GLboolean cullEnabled = GL_FALSE;
    GLboolean depthEnabled = GL_FALSE;
    GLboolean scissorEnabled = GL_FALSE;

    GlStateGuard() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        if (g_gl.activeTexture != nullptr) {
            g_gl.activeTexture(GL_TEXTURE0);
        }
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding2D);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEqRgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEqAlpha);
        blendEnabled = glIsEnabled(GL_BLEND);
        cullEnabled = glIsEnabled(GL_CULL_FACE);
        depthEnabled = glIsEnabled(GL_DEPTH_TEST);
        scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    }

    ~GlStateGuard() {
        if (blendEnabled == GL_TRUE) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (cullEnabled == GL_TRUE) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        if (depthEnabled == GL_TRUE) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (scissorEnabled == GL_TRUE) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);

        g_gl.blendEquationSeparate(static_cast<GLenum>(blendEqRgb), static_cast<GLenum>(blendEqAlpha));
        g_gl.blendFuncSeparate(static_cast<GLenum>(blendSrcRgb), static_cast<GLenum>(blendDstRgb),
                               static_cast<GLenum>(blendSrcAlpha), static_cast<GLenum>(blendDstAlpha));
        g_gl.useProgram(static_cast<GLuint>(program));
        g_gl.bindVertexArray(static_cast<GLuint>(vertexArray));
        g_gl.bindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));
        g_gl.activeTexture(static_cast<GLenum>(activeTexture));
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textureBinding2D));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    }
};
} // namespace

bool EspOverlayRenderer::EnsureInitialized(HGLRC currentContext) {
    if (currentContext == nullptr) {
        return false;
    }

    if (renderContext_ == currentContext && program_ != 0 && vertexArray_ != 0 && vertexBuffer_ != 0) {
        return true;
    }

    if (renderContext_ != nullptr && renderContext_ != currentContext) {
        AbandonContext();
    }

    if (!LoadGlFunctions()) {
        if (!loggedInitializationFailure_) {
            loggedInitializationFailure_ = true;
            std::cout << "[ESP-GL] Failed to resolve required OpenGL functions." << std::endl;
        }
        return false;
    }

    static constexpr const char *kVertexShader = R"(#version 150
in vec2 aPosition;
in vec4 aColor;
out vec4 vColor;
void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vColor = aColor;
})";

    static constexpr const char *kFragmentShader = R"(#version 150
in vec4 vColor;
out vec4 outColor;
void main() {
    outColor = vColor;
})";

    const GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) g_gl.deleteShader(vertexShader);
        if (fragmentShader != 0) g_gl.deleteShader(fragmentShader);
        loggedInitializationFailure_ = true;
        return false;
    }

    const GLuint program = g_gl.createProgram();
    g_gl.attachShader(program, vertexShader);
    g_gl.attachShader(program, fragmentShader);
    g_gl.bindAttribLocation(program, 0, "aPosition");
    g_gl.bindAttribLocation(program, 1, "aColor");
    g_gl.linkProgram(program);

    GLint linkStatus = GL_FALSE;
    g_gl.getProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        char infoLog[512]{};
        GLsizei infoLength = 0;
        g_gl.getProgramInfoLog(program, static_cast<GLsizei>(sizeof(infoLog)), &infoLength, infoLog);
        std::cout << "[ESP-GL] Program link failed: " << (infoLength > 0 ? infoLog : "<no info>") << std::endl;
        g_gl.detachShader(program, vertexShader);
        g_gl.detachShader(program, fragmentShader);
        g_gl.deleteShader(vertexShader);
        g_gl.deleteShader(fragmentShader);
        g_gl.deleteProgram(program);
        loggedInitializationFailure_ = true;
        return false;
    }

    g_gl.detachShader(program, vertexShader);
    g_gl.detachShader(program, fragmentShader);
    g_gl.deleteShader(vertexShader);
    g_gl.deleteShader(fragmentShader);

    GLuint vao = 0;
    GLuint vbo = 0;
    g_gl.genVertexArrays(1, &vao);
    g_gl.genBuffers(1, &vbo);
    if (vao == 0 || vbo == 0) {
        if (vao != 0) g_gl.deleteVertexArrays(1, &vao);
        if (vbo != 0) g_gl.deleteBuffers(1, &vbo);
        g_gl.deleteProgram(program);
        loggedInitializationFailure_ = true;
        std::cout << "[ESP-GL] Failed to allocate ESP renderer buffers." << std::endl;
        return false;
    }

    g_gl.bindVertexArray(vao);
    g_gl.bindBuffer(GL_ARRAY_BUFFER, vbo);
    g_gl.enableVertexAttribArray(0);
    g_gl.enableVertexAttribArray(1);
    g_gl.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(OverlayVertex),
                             reinterpret_cast<const void *>(offsetof(OverlayVertex, x)));
    g_gl.vertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(OverlayVertex),
                             reinterpret_cast<const void *>(offsetof(OverlayVertex, r)));
    g_gl.bindBuffer(GL_ARRAY_BUFFER, 0);
    g_gl.bindVertexArray(0);

    renderContext_ = currentContext;
    program_ = program;
    vertexArray_ = vao;
    vertexBuffer_ = vbo;
    loggedInitializationFailure_ = false;
    return true;
}

void EspOverlayRenderer::DestroyResources() {
    if (program_ != 0 && g_gl.deleteProgram != nullptr) {
        g_gl.deleteProgram(program_);
    }
    if (vertexBuffer_ != 0 && g_gl.deleteBuffers != nullptr) {
        g_gl.deleteBuffers(1, &vertexBuffer_);
    }
    if (vertexArray_ != 0 && g_gl.deleteVertexArrays != nullptr) {
        g_gl.deleteVertexArrays(1, &vertexArray_);
    }

    program_ = 0;
    vertexBuffer_ = 0;
    vertexArray_ = 0;
    smoothedTargets_.clear();
}

bool EspOverlayRenderer::Render(HGLRC currentContext, const Esp::RenderSnapshot &snapshot,
                                const Esp::RenderCameraState &renderCameraState, bool drawTracer, bool drawBox,
                                const float *tracerColor, float tracerThickness, const float *boxColor,
                                float boxThickness) {
    if (currentContext == nullptr || !snapshot.initialized || (!drawTracer && !drawBox)) {
        return false;
    }

    if (!EnsureInitialized(currentContext)) {
        return false;
    }

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float width = static_cast<float>(viewport[2]);
    const float height = static_cast<float>(viewport[3]);
    if (width <= 0.0f || height <= 0.0f) {
        return false;
    }

    if (!renderCameraState.valid || !renderCameraState.cameraState.valid) {
        return false;
    }
    const Esp::CameraState &camera = renderCameraState.cameraState;
    const float alpha = std::clamp(renderCameraState.interpolationAlpha, 0.0f, 1.0f);

    const Color4f tracerDrawColor = ClampColor(tracerColor, {1.0f, 70.0f / 255.0f, 70.0f / 255.0f, 220.0f / 255.0f});
    const Color4f boxDrawColor = ClampColor(boxColor, {1.0f, 70.0f / 255.0f, 70.0f / 255.0f, 210.0f / 255.0f});
    const float tracerLineThickness = std::clamp(tracerThickness, 0.5f, 8.0f);
    const float boxLineThickness = std::clamp(boxThickness, 0.5f, 8.0f);
    const float tracerPointRadius = std::max(2.0f, tracerLineThickness + 1.1f);
    const Vec2f tracerStart{width * 0.5f, height - 42.0f};
    const double nowSeconds = GetHighResolutionTimeSeconds();

    std::vector<OverlayVertex> vertices;
    vertices.reserve(snapshot.targets.size() * 40);

    for (const Esp::Target &target : snapshot.targets) {
        const double rawWorldX = Lerp(target.previousX, target.x, alpha);
        const double rawWorldBaseY = Lerp(target.previousY, target.y, alpha);
        const double rawWorldZ = Lerp(target.previousZ, target.z, alpha);

        double worldX = rawWorldX;
        double worldBaseY = rawWorldBaseY;
        double worldZ = rawWorldZ;
        if (target.entityId != 0) {
            SmoothedTargetState &smoothedTarget = smoothedTargets_[target.entityId];
            if (!smoothedTarget.initialized) {
                smoothedTarget.x = rawWorldX;
                smoothedTarget.y = rawWorldBaseY;
                smoothedTarget.z = rawWorldZ;
                smoothedTarget.initialized = true;
            } else {
                const double errorX = rawWorldX - smoothedTarget.x;
                const double errorY = rawWorldBaseY - smoothedTarget.y;
                const double errorZ = rawWorldZ - smoothedTarget.z;
                const double trackingError = std::sqrt(errorX * errorX + errorY * errorY + errorZ * errorZ);
                const double smoothingAlpha = ComputeSmoothingAlpha(nowSeconds - smoothedTarget.lastUpdateSeconds,
                                                                   trackingError);
                smoothedTarget.x += (rawWorldX - smoothedTarget.x) * smoothingAlpha;
                smoothedTarget.y += (rawWorldBaseY - smoothedTarget.y) * smoothingAlpha;
                smoothedTarget.z += (rawWorldZ - smoothedTarget.z) * smoothingAlpha;
            }

            smoothedTarget.lastUpdateSeconds = nowSeconds;
            smoothedTarget.lastSeenSeconds = nowSeconds;
            worldX = smoothedTarget.x;
            worldBaseY = smoothedTarget.y;
            worldZ = smoothedTarget.z;
        }
        const double worldY = worldBaseY + target.eyeHeightOffset;

        Vec2f tracerEnd{};
        if (drawTracer &&
            WorldToScreen({worldX, worldY - 0.60, worldZ}, camera, width, height, tracerEnd)) {
            PushThickLine(vertices, tracerStart, tracerEnd, tracerLineThickness, width, height, tracerDrawColor);
            PushPointQuad(vertices, tracerEnd, tracerPointRadius, width, height, tracerDrawColor);
        }

        if (drawBox) {
            double headNdcX{};
            double headNdcY{};
            double feetNdcX{};
            double feetNdcY{};
            const bool headVisible = WorldToScreenVisible({worldX, worldBaseY + target.eyeHeightOffset + 0.18, worldZ},
                                                          camera, width, height, headNdcX, headNdcY);
            const bool feetVisible = WorldToScreenVisible({worldX, worldBaseY - 0.08, worldZ},
                                                          camera, width, height, feetNdcX, feetNdcY);
            if (!headVisible || !feetVisible) {
                continue;
            }

            const Vec2f headScreen{
                static_cast<float>((headNdcX + 1.0) * 0.5 * width),
                static_cast<float>((1.0 - headNdcY) * 0.5 * height),
            };
            const Vec2f feetScreen{
                static_cast<float>((feetNdcX + 1.0) * 0.5 * width),
                static_cast<float>((1.0 - feetNdcY) * 0.5 * height),
            };

            const float top = std::min(headScreen.y, feetScreen.y);
            const float bottom = std::max(headScreen.y, feetScreen.y);
            const float boxHeight = bottom - top;
            if (boxHeight < 6.0f) {
                continue;
            }

            const float centerX = (headScreen.x + feetScreen.x) * 0.5f;
            const float halfWidth = boxHeight * 0.19f;
            const float left = centerX - halfWidth;
            const float right = centerX + halfWidth;

            PushThickLine(vertices, {left, top}, {right, top}, boxLineThickness, width, height, boxDrawColor);
            PushThickLine(vertices, {right, top}, {right, bottom}, boxLineThickness, width, height, boxDrawColor);
            PushThickLine(vertices, {right, bottom}, {left, bottom}, boxLineThickness, width, height, boxDrawColor);
            PushThickLine(vertices, {left, bottom}, {left, top}, boxLineThickness, width, height, boxDrawColor);
        }
    }

    for (auto it = smoothedTargets_.begin(); it != smoothedTargets_.end();) {
        if ((nowSeconds - it->second.lastSeenSeconds) > 1.0) {
            it = smoothedTargets_.erase(it);
            continue;
        }
        ++it;
    }

    if (vertices.empty()) {
        return true;
    }

    GlStateGuard stateGuard;
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    g_gl.blendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    g_gl.blendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    g_gl.activeTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.useProgram(program_);
    g_gl.bindVertexArray(vertexArray_);
    g_gl.bindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    g_gl.bufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(OverlayVertex)),
                    vertices.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    return true;
}

void EspOverlayRenderer::ShutdownForCurrentContext(HGLRC currentContext) {
    if (renderContext_ == nullptr || currentContext == nullptr || currentContext != renderContext_) {
        return;
    }

    DestroyResources();
    renderContext_ = nullptr;
}

void EspOverlayRenderer::AbandonContext() {
    DestroyResources();
    renderContext_ = nullptr;
}
