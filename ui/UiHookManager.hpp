#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <windows.h>

#include "EspOverlayRenderer.hpp"
#include "ImGuiMenu.hpp"

class GameplayController {
public:
    virtual ~GameplayController() = default;
    virtual void OnRenderFrame(const ImGuiMenuState &state) = 0;
};

enum class UiHookRenderStage : uint32_t {
    Idle = 0,
    FrameStart = 1,
    EspRender = 2,
    MenuRender = 3,
    FrameEnd = 4,
};

struct UiHookDebugSnapshot {
    DWORD renderThreadId = 0;
    HWND window = nullptr;
    HGLRC renderContext = nullptr;
    uint32_t featureMask = 0;
    UiHookRenderStage renderStage = UiHookRenderStage::Idle;
    bool insideSwapHook = false;
    bool menuInitialized = false;
    bool shuttingDown = false;
};

class UiHookManager {
public:
    UiHookManager() = default;
    UiHookManager(const UiHookManager &) = delete;
    UiHookManager &operator=(const UiHookManager &) = delete;

    bool Initialize();
    void Shutdown();
    void SetEsp(Esp *esp);
    void SetGameplayController(GameplayController *controller);

    ImGuiMenuState GetStateSnapshot() const;
    [[nodiscard]] static UiHookDebugSnapshot GetDebugSnapshot();

private:
    using SwapBuffersFn = BOOL(WINAPI *)(HDC);
    using WndProcFn = LRESULT(CALLBACK *)(HWND, UINT, WPARAM, LPARAM);

    bool TryInstallHooks();
    bool AttachToWindow(HWND hWnd);
    void DetachWndProc();
    bool IsValidGameWindow(HWND hWnd) const;
    HWND ResolveGameWindow(HDC hDc) const;
    bool ObserveStableRenderTarget(HWND hWnd, HGLRC context);
    void ResetObservedRenderTarget();
    bool PerformRenderThreadShutdown(HGLRC currentContext);
    void ResetBoundRenderer(bool resetMenuVisibility);
    bool WaitForRenderThreadShutdown();
    void WaitForCallbacksToDrain() const;
    BOOL RenderMenuAndCallOriginal(HDC hDc, SwapBuffersFn originalFn, const char *hookName);
    void UpdateDebugSnapshot(const ImGuiMenuState &state, bool insideSwapHook,
                             UiHookRenderStage renderStage = UiHookRenderStage::Idle);
    static uint32_t BuildFeatureMask(const ImGuiMenuState &state);
    static void ResetDebugSnapshot();

    static BOOL WINAPI HookedSwapBuffers(HDC hDc);
    static BOOL WINAPI HookedWglSwapBuffers(HDC hDc);
    static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static UiHookManager *instance_;
    static thread_local bool inSwapHook_;
    static std::atomic_uint32_t lastRenderThreadId_;
    static std::atomic_uintptr_t lastWindow_;
    static std::atomic_uintptr_t lastRenderContext_;
    static std::atomic_uint32_t lastFeatureMask_;
    static std::atomic_uint32_t lastRenderStage_;
    static std::atomic_bool lastInsideSwapHook_;
    static std::atomic_bool lastMenuInitialized_;
    static std::atomic_bool lastShuttingDown_;

    ImGuiMenu menu_;
    GameplayController *gameplayController_ = nullptr;
    Esp *esp_ = nullptr;
    EspOverlayRenderer espOverlayRenderer_{};
    HWND window_ = nullptr;
    HGLRC renderContext_ = nullptr;
    HWND observedWindow_ = nullptr;
    HGLRC observedRenderContext_ = nullptr;
    unsigned int observedRenderTargetHits_ = 0;
    void *swapBuffersTarget_ = nullptr;
    void *wglSwapBuffersTarget_ = nullptr;
    SwapBuffersFn originalSwapBuffers_ = nullptr;
    SwapBuffersFn originalWglSwapBuffers_ = nullptr;
    WndProcFn originalWndProc_ = nullptr;
    bool minHookInitialized_ = false;
    bool wndProcHooked_ = false;
    bool swapHookInstalled_ = false;
    bool wglHookInstalled_ = false;
    bool loggedMissingContext_ = false;
    bool loggedFirstRenderCallback_ = false;
    bool loggedRenderThreadShutdownWait_ = false;
    bool loggedSingleHookStrategy_ = false;
    std::atomic_bool shuttingDown_ = false;
    std::atomic_bool renderThreadShutdownComplete_ = false;
    std::atomic_uint32_t activeSwapCalls_ = 0;
    std::atomic_uint32_t activeWndProcCalls_ = 0;
    mutable std::recursive_mutex renderMutex_;
};
