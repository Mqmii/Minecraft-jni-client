#include "UiHookManager.hpp"

#include <iostream>

#include "MinHook.h"

UiHookManager *UiHookManager::instance_ = nullptr;
thread_local bool UiHookManager::inSwapHook_ = false;

namespace {
class CallbackScope {
public:
    explicit CallbackScope(std::atomic_uint32_t &counter) : counter_(counter) {
        counter_.fetch_add(1, std::memory_order_acq_rel);
    }

    ~CallbackScope() {
        counter_.fetch_sub(1, std::memory_order_acq_rel);
    }

private:
    std::atomic_uint32_t &counter_;
};
} // namespace

bool UiHookManager::Initialize() {
    if (instance_ != nullptr && instance_ != this) {
        std::cout << "[!] ERROR: UiHookManager is already active." << std::endl;
        return false;
    }

    instance_ = this;
    shuttingDown_ = false;

    const MH_STATUS initStatus = MH_Initialize();
    if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED) {
        std::cout << "[!] ERROR: Failed to initialize MinHook." << std::endl;
        return false;
    }
    minHookInitialized_ = true;

    constexpr int kMaxAttempts = 50;
    bool anyHookEnabled = false;
    for (int attempt = 0; attempt < kMaxAttempts && !anyHookEnabled; ++attempt) {
        anyHookEnabled = TryInstallHooks();
        if (!anyHookEnabled) {
            Sleep(100);
        }
    }

    if (anyHookEnabled) {
        std::cout << "[+] MENU: Press INSERT to toggle." << std::endl;
    } else {
        std::cout << "[!] ERROR: No active swap hook; menu cannot be rendered." << std::endl;
        MH_Uninitialize();
        minHookInitialized_ = false;
        instance_ = nullptr;
    }

    return anyHookEnabled;
}

bool UiHookManager::TryInstallHooks() {
    bool anyHookEnabled = false;

    if (!swapHookInstalled_) {
        if (swapBuffersTarget_ == nullptr) {
            if (HMODULE gdiModule = GetModuleHandleW(L"gdi32.dll"); gdiModule != nullptr) {
                swapBuffersTarget_ = reinterpret_cast<void *>(GetProcAddress(gdiModule, "SwapBuffers"));
            }
        }

        if (swapBuffersTarget_ != nullptr) {
            const MH_STATUS createStatus = MH_CreateHook(
                swapBuffersTarget_,
                reinterpret_cast<void *>(&UiHookManager::HookedSwapBuffers),
                reinterpret_cast<void **>(&originalSwapBuffers_));
            if (createStatus == MH_OK || createStatus == MH_ERROR_ALREADY_CREATED) {
                const MH_STATUS enableStatus = MH_EnableHook(swapBuffersTarget_);
                if (enableStatus == MH_OK || enableStatus == MH_ERROR_ENABLED) {
                    swapHookInstalled_ = true;
                    anyHookEnabled = true;
                    std::cout << "[+] gdi32!SwapBuffers hook installed." << std::endl;
                }
            }
        }
    } else {
        anyHookEnabled = true;
    }

    if (!wglHookInstalled_) {
        if (wglSwapBuffersTarget_ == nullptr) {
            if (HMODULE openglModule = GetModuleHandleW(L"opengl32.dll"); openglModule != nullptr) {
                wglSwapBuffersTarget_ = reinterpret_cast<void *>(GetProcAddress(openglModule, "wglSwapBuffers"));
            }
        }

        if (wglSwapBuffersTarget_ != nullptr) {
            const MH_STATUS createStatus = MH_CreateHook(
                wglSwapBuffersTarget_,
                reinterpret_cast<void *>(&UiHookManager::HookedWglSwapBuffers),
                reinterpret_cast<void **>(&originalWglSwapBuffers_));
            if (createStatus == MH_OK || createStatus == MH_ERROR_ALREADY_CREATED) {
                const MH_STATUS enableStatus = MH_EnableHook(wglSwapBuffersTarget_);
                if (enableStatus == MH_OK || enableStatus == MH_ERROR_ENABLED) {
                    wglHookInstalled_ = true;
                    anyHookEnabled = true;
                    std::cout << "[+] opengl32!wglSwapBuffers hook installed." << std::endl;
                }
            }
        }
    } else {
        anyHookEnabled = true;
    }

    return anyHookEnabled;
}

void UiHookManager::Shutdown() {
    shuttingDown_ = true;

    if (minHookInitialized_) {
        if (swapHookInstalled_ && swapBuffersTarget_ != nullptr) {
            MH_DisableHook(swapBuffersTarget_);
            MH_RemoveHook(swapBuffersTarget_);
            swapHookInstalled_ = false;
        }
        if (wglHookInstalled_ && wglSwapBuffersTarget_ != nullptr) {
            MH_DisableHook(wglSwapBuffersTarget_);
            MH_RemoveHook(wglSwapBuffersTarget_);
            wglHookInstalled_ = false;
        }
    }

    DetachWndProc();
    WaitForCallbacksToDrain();

    if (menu_.IsInitialized()) {
        const bool canShutdownRenderer = renderContext_ != nullptr && wglGetCurrentContext() == renderContext_;
        menu_.Shutdown(canShutdownRenderer);
    }

    if (minHookInitialized_) {
        MH_Uninitialize();
        minHookInitialized_ = false;
    }

    window_ = nullptr;
    renderContext_ = nullptr;
    ResetObservedRenderTarget();
    swapBuffersTarget_ = nullptr;
    wglSwapBuffersTarget_ = nullptr;
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

bool UiHookManager::AttachToWindow(HWND hWnd) {
    if (!IsValidGameWindow(hWnd)) {
        return false;
    }

    if (window_ != hWnd) {
        if (menu_.IsInitialized()) {
            const bool canShutdownRenderer = renderContext_ != nullptr && wglGetCurrentContext() == renderContext_;
            menu_.Shutdown(canShutdownRenderer);
        }
        renderContext_ = nullptr;
        DetachWndProc();
        window_ = hWnd;
    }

    if (wndProcHooked_) {
        return true;
    }

    SetLastError(0);
    auto currentWndProc = reinterpret_cast<WndProcFn>(GetWindowLongPtr(window_, GWLP_WNDPROC));
    if (currentWndProc == nullptr && GetLastError() != 0) {
        std::cout << "[!] ERROR: Failed to query WndProc." << std::endl;
        return false;
    }

    SetLastError(0);
    const auto previousWndProc = reinterpret_cast<WndProcFn>(
        SetWindowLongPtr(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&UiHookManager::HookedWndProc)));
    if (previousWndProc == nullptr && GetLastError() != 0) {
        std::cout << "[!] ERROR: Failed to hook WndProc." << std::endl;
        return false;
    }

    originalWndProc_ = currentWndProc != nullptr ? currentWndProc : previousWndProc;
    wndProcHooked_ = originalWndProc_ != nullptr;
    return wndProcHooked_;
}

void UiHookManager::DetachWndProc() {
    if (wndProcHooked_ && window_ != nullptr && originalWndProc_ != nullptr) {
        SetWindowLongPtr(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWndProc_));
    }

    wndProcHooked_ = false;
    originalWndProc_ = nullptr;
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

HWND UiHookManager::ResolveGameWindow(HDC hDc) const {
    HWND candidateWindow = WindowFromDC(hDc);
    if (candidateWindow == nullptr) {
        return nullptr;
    }

    if (HWND rootWindow = GetAncestor(candidateWindow, GA_ROOT); rootWindow != nullptr) {
        candidateWindow = rootWindow;
    }

    return IsValidGameWindow(candidateWindow) ? candidateWindow : nullptr;
}

bool UiHookManager::ObserveStableRenderTarget(HWND hWnd, HGLRC context) {
    if (hWnd == nullptr || context == nullptr) {
        ResetObservedRenderTarget();
        return false;
    }

    HWND foregroundWindow = GetForegroundWindow();
    if (foregroundWindow != nullptr) {
        if (HWND foregroundRoot = GetAncestor(foregroundWindow, GA_ROOT); foregroundRoot != nullptr) {
            foregroundWindow = foregroundRoot;
        }
    }
    if (foregroundWindow != nullptr && foregroundWindow != hWnd) {
        ResetObservedRenderTarget();
        return false;
    }

    if (observedWindow_ == hWnd && observedRenderContext_ == context) {
        ++observedRenderTargetHits_;
    } else {
        observedWindow_ = hWnd;
        observedRenderContext_ = context;
        observedRenderTargetHits_ = 1;
    }

    constexpr unsigned int kRequiredStableHits = 3;
    return observedRenderTargetHits_ >= kRequiredStableHits;
}

void UiHookManager::ResetObservedRenderTarget() {
    observedWindow_ = nullptr;
    observedRenderContext_ = nullptr;
    observedRenderTargetHits_ = 0;
}

void UiHookManager::WaitForCallbacksToDrain() const {
    constexpr int kMaxWaitIterations = 200;
    for (int attempt = 0; attempt < kMaxWaitIterations; ++attempt) {
        if (activeSwapCalls_.load(std::memory_order_acquire) == 0 &&
            activeWndProcCalls_.load(std::memory_order_acquire) == 0) {
            return;
        }
        Sleep(10);
    }

    std::cout << "[!] WARNING: Timed out waiting for UI callbacks to drain." << std::endl;
}

BOOL UiHookManager::RenderMenuAndCallOriginal(HDC hDc, SwapBuffersFn originalFn) {
    if (originalFn == nullptr) {
        return FALSE;
    }

    CallbackScope callbackScope(activeSwapCalls_);

    if (shuttingDown_) {
        return originalFn(hDc);
    }

    if (inSwapHook_) {
        return originalFn(hDc);
    }
    inSwapHook_ = true;
    struct SwapHookReset {
        bool &flag;
        ~SwapHookReset() { flag = false; }
    } reset{inSwapHook_};

    const HGLRC currentContext = wglGetCurrentContext();
    if (currentContext == nullptr) {
        return originalFn(hDc);
    }

    if (HWND candidateWindow = ResolveGameWindow(hDc); candidateWindow != nullptr) {
        const bool isBoundRenderTarget = window_ == candidateWindow && renderContext_ == currentContext;
        if (!isBoundRenderTarget) {
            if (!ObserveStableRenderTarget(candidateWindow, currentContext)) {
                return originalFn(hDc);
            }

            if (!AttachToWindow(candidateWindow)) {
                return originalFn(hDc);
            }

            if (menu_.IsInitialized()) {
                const bool canShutdownRenderer = renderContext_ != nullptr && wglGetCurrentContext() == renderContext_;
                menu_.Shutdown(canShutdownRenderer);
            }

            renderContext_ = currentContext;
            ResetObservedRenderTarget();
        }

        if (window_ == candidateWindow && renderContext_ == currentContext) {
            if (!menu_.IsInitialized()) {
                if (!menu_.Initialize(window_)) {
                    std::cout << "[!] ERROR: Failed to initialize ImGui." << std::endl;
                    return originalFn(hDc);
                }
            }

            menu_.RenderFrame();
        }
    }

    return originalFn(hDc);
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
    UiHookManager *instance = instance_;
    if (instance == nullptr) {
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    CallbackScope callbackScope(instance->activeWndProcCalls_);

    if (!instance->shuttingDown_ && instance->menu_.HandleWndProc(hWnd, uMsg, wParam, lParam)) {
        return 0;
    }

    if (instance->originalWndProc_ == nullptr) {
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return CallWindowProc(instance->originalWndProc_, hWnd, uMsg, wParam, lParam);
}
