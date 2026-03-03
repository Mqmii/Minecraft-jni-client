#pragma once

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

    bool IsValidGameWindow(HWND hWnd) const;
    BOOL RenderMenuAndCallOriginal(HDC hDc, SwapBuffersFn originalFn);

    static BOOL WINAPI HookedSwapBuffers(HDC hDc);
    static BOOL WINAPI HookedWglSwapBuffers(HDC hDc);
    static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static UiHookManager *instance_;
    static thread_local bool inSwapHook_;

    ImGuiMenu menu_;
    HWND window_ = nullptr;
    SwapBuffersFn originalSwapBuffers_ = nullptr;
    SwapBuffersFn originalWglSwapBuffers_ = nullptr;
    WndProcFn originalWndProc_ = nullptr;
    bool minHookInitialized_ = false;
    bool wndProcHooked_ = false;
};
