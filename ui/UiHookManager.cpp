#include "UiHookManager.hpp"

#include <iostream>

#include "MinHook.h"

UiHookManager *UiHookManager::instance_ = nullptr;
thread_local bool UiHookManager::inSwapHook_ = false;

bool UiHookManager::Initialize() {
    if (instance_ != nullptr && instance_ != this) {
        std::cout << "[!] ERROR: UiHookManager is already active." << std::endl;
        return false;
    }

    instance_ = this;

    const MH_STATUS initStatus = MH_Initialize();
    if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED) {
        std::cout << "[!] ERROR: Failed to initialize MinHook." << std::endl;
        return false;
    }
    minHookInitialized_ = true;

    const auto gdiSwapAddr = reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"gdi32.dll"), "SwapBuffers"));
    const auto wglSwapAddr = reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"opengl32.dll"), "wglSwapBuffers"));

    bool anyHookEnabled = false;

    if (gdiSwapAddr != nullptr &&
        MH_CreateHook(gdiSwapAddr, reinterpret_cast<void *>(&UiHookManager::HookedSwapBuffers),
                      reinterpret_cast<void **>(&originalSwapBuffers_)) == MH_OK &&
        MH_EnableHook(gdiSwapAddr) == MH_OK) {
        anyHookEnabled = true;
        std::cout << "[+] gdi32!SwapBuffers hook installed." << std::endl;
    } else {
        std::cout << "[!] WARNING: Failed to hook gdi32!SwapBuffers." << std::endl;
    }

    if (wglSwapAddr != nullptr &&
        MH_CreateHook(wglSwapAddr, reinterpret_cast<void *>(&UiHookManager::HookedWglSwapBuffers),
                      reinterpret_cast<void **>(&originalWglSwapBuffers_)) == MH_OK &&
        MH_EnableHook(wglSwapAddr) == MH_OK) {
        anyHookEnabled = true;
        std::cout << "[+] opengl32!wglSwapBuffers hook installed." << std::endl;
    } else {
        std::cout << "[!] WARNING: Failed to hook opengl32!wglSwapBuffers." << std::endl;
    }

    if (anyHookEnabled) {
        std::cout << "[+] MENU: Press INSERT to toggle." << std::endl;
    } else {
        std::cout << "[!] ERROR: No active swap hook; menu cannot be rendered." << std::endl;
    }

    return anyHookEnabled;
}

void UiHookManager::Shutdown() {
    if (minHookInitialized_) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        minHookInitialized_ = false;
    }

    if (wndProcHooked_ && window_ != nullptr && originalWndProc_ != nullptr) {
        SetWindowLongPtr(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWndProc_));
        wndProcHooked_ = false;
    }

    if (menu_.IsInitialized()) {
        menu_.Shutdown();
    }

    window_ = nullptr;
    originalWndProc_ = nullptr;
    originalSwapBuffers_ = nullptr;
    originalWglSwapBuffers_ = nullptr;

    if (instance_ == this) {
        instance_ = nullptr;
    }
}

ImGuiMenuState &UiHookManager::State() {
    return menu_.State();
}

const ImGuiMenuState &UiHookManager::State() const {
    return menu_.State();
}

bool UiHookManager::IsValidGameWindow(HWND hWnd) const {
    if (hWnd == nullptr || !IsWindow(hWnd) || !IsWindowVisible(hWnd)) {
        return false;
    }

    RECT rc{};
    if (!GetClientRect(hWnd, &rc)) {
        return false;
    }

    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    return width > 0 && height > 0;
}

BOOL UiHookManager::RenderMenuAndCallOriginal(HDC hDc, SwapBuffersFn originalFn) {
    if (originalFn == nullptr) {
        return FALSE;
    }

    if (inSwapHook_) {
        return originalFn(hDc);
    }
    inSwapHook_ = true;

    if (!menu_.IsInitialized()) {
        const HWND candidateWindow = WindowFromDC(hDc);
        if (IsValidGameWindow(candidateWindow)) {
            if (!wndProcHooked_) {
                window_ = candidateWindow;
                SetLastError(0);

                auto prevWndProc = reinterpret_cast<WndProcFn>(
                    SetWindowLongPtr(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&UiHookManager::HookedWndProc)));
                if (prevWndProc != nullptr) {
                    originalWndProc_ = prevWndProc;
                    wndProcHooked_ = true;
                } else if (GetLastError() != 0) {
                    std::cout << "[!] ERROR: Failed to hook WndProc." << std::endl;
                }
            }

            if (!menu_.Initialize(window_)) {
                std::cout << "[!] ERROR: Failed to initialize ImGui." << std::endl;
            }
        }
    }

    if (menu_.IsInitialized()) {
        menu_.RenderFrame();
    }

    const BOOL result = originalFn(hDc);
    inSwapHook_ = false;
    return result;
}

BOOL WINAPI UiHookManager::HookedSwapBuffers(HDC hDc) {
    if (instance_ == nullptr) {
        return FALSE;
    }
    return instance_->RenderMenuAndCallOriginal(hDc, instance_->originalSwapBuffers_);
}

BOOL WINAPI UiHookManager::HookedWglSwapBuffers(HDC hDc) {
    if (instance_ == nullptr) {
        return FALSE;
    }
    return instance_->RenderMenuAndCallOriginal(hDc, instance_->originalWglSwapBuffers_);
}

LRESULT CALLBACK UiHookManager::HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (instance_ != nullptr && instance_->menu_.HandleWndProc(hWnd, uMsg, wParam, lParam)) {
        return 0;
    }

    if (instance_ == nullptr || instance_->originalWndProc_ == nullptr) {
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return CallWindowProc(instance_->originalWndProc_, hWnd, uMsg, wParam, lParam);
}
