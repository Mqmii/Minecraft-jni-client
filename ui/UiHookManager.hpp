#pragma once

#include <atomic>
#include <windows.h>

#include "ImGuiMenu.hpp"

class UiHookManager {
public:
    UiHookManager() = default;
    UiHookManager(const UiHookManager &) = delete;
    UiHookManager &operator=(const UiHookManager &) = delete;

    bool Initialize();
    void Shutdown();

    ImGuiMenuState &State();
    const ImGuiMenuState &State() const;

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
    void WaitForCallbacksToDrain() const;
    BOOL RenderMenuAndCallOriginal(HDC hDc, SwapBuffersFn originalFn);

    static BOOL WINAPI HookedSwapBuffers(HDC hDc);
    static BOOL WINAPI HookedWglSwapBuffers(HDC hDc);
    static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static UiHookManager *instance_;
    static thread_local bool inSwapHook_;

    ImGuiMenu menu_;
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
    std::atomic_bool shuttingDown_ = false;
    std::atomic_uint32_t activeSwapCalls_ = 0;
    std::atomic_uint32_t activeWndProcCalls_ = 0;
};
