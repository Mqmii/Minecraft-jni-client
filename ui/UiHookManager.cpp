#include "UiHookManager.hpp"

#include <cstdint>
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
    renderThreadShutdownComplete_ = false;
    loggedFirstRenderCallback_ = false;
    loggedRenderThreadShutdownWait_ = false;
    loggedSingleHookStrategy_ = false;
    menu_.SetToggleKeyPollingEnabled(true);

    const MH_STATUS initStatus = MH_Initialize();
    if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED) {
        std::cout << "[!] ERROR: Failed to initialize MinHook." << std::endl;
        instance_ = nullptr;
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

    if (wglHookInstalled_) {
        if (!loggedSingleHookStrategy_) {
            std::cout << "[UI] Using wglSwapBuffers as the primary render hook." << std::endl;
            loggedSingleHookStrategy_ = true;
        }
        return true;
    }

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
                    std::cout << "[UI] Falling back to gdi32!SwapBuffers hook." << std::endl;
                }
            }
        }
    } else {
        anyHookEnabled = true;
    }

    return anyHookEnabled;
}

void UiHookManager::Shutdown() {
    std::cout << "[UI] Shutdown requested." << std::endl;
    shuttingDown_ = true;
    menu_.SetRunning(false);

    const bool renderThreadShutdownCompleted = WaitForRenderThreadShutdown();
    if (!renderThreadShutdownCompleted && menu_.IsInitialized()) {
        std::cout << "[!] WARNING: Render-thread shutdown did not complete before unload." << std::endl;
    }

    if (minHookInitialized_) {
        if (swapHookInstalled_ && swapBuffersTarget_ != nullptr) {
            MH_DisableHook(swapBuffersTarget_);
        }
        if (wglHookInstalled_ && wglSwapBuffersTarget_ != nullptr) {
            MH_DisableHook(wglSwapBuffersTarget_);
        }
    }

    DetachWndProc();
    WaitForCallbacksToDrain();

    if (minHookInitialized_) {
        if (swapHookInstalled_ && swapBuffersTarget_ != nullptr) {
            MH_RemoveHook(swapBuffersTarget_);
            swapHookInstalled_ = false;
        }
        if (wglHookInstalled_ && wglSwapBuffersTarget_ != nullptr) {
            MH_RemoveHook(wglSwapBuffersTarget_);
            wglHookInstalled_ = false;
        }
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
    std::cout << "[UI] Shutdown complete." << std::endl;
}

void UiHookManager::SetEsp(Esp *esp) {
    std::scoped_lock renderLock(renderMutex_);
    menu_.SetEsp(esp);
}

ImGuiMenuState UiHookManager::GetStateSnapshot() const {
    return menu_.GetStateSnapshot();
}

bool UiHookManager::AttachToWindow(HWND hWnd) {
    if (!IsValidGameWindow(hWnd)) {
        return false;
    }

    if (window_ != hWnd) {
        std::cout << "[UI] Binding to window 0x" << std::hex << reinterpret_cast<uintptr_t>(hWnd) << std::dec
                  << std::endl;
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
    if (wndProcHooked_) {
        menu_.SetToggleKeyPollingEnabled(false);
        std::cout << "[UI] WndProc hook active for window 0x" << std::hex << reinterpret_cast<uintptr_t>(window_)
                  << std::dec << std::endl;
    }
    return wndProcHooked_;
}

void UiHookManager::DetachWndProc() {
    if (wndProcHooked_ && window_ != nullptr && originalWndProc_ != nullptr) {
        SetLastError(0);
        auto currentWndProc = reinterpret_cast<WndProcFn>(GetWindowLongPtr(window_, GWLP_WNDPROC));
        if (currentWndProc == reinterpret_cast<WndProcFn>(&UiHookManager::HookedWndProc)) {
            SetWindowLongPtr(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWndProc_));
        } else if (currentWndProc != originalWndProc_) {
            std::cout << "[!] WARNING: Skipping WndProc restore because the chain changed." << std::endl;
        }
    }

    wndProcHooked_ = false;
    originalWndProc_ = nullptr;
    menu_.SetToggleKeyPollingEnabled(true);
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
        std::cout << "[UI] Observing render target window=0x" << std::hex << reinterpret_cast<uintptr_t>(hWnd)
                  << " context=0x" << reinterpret_cast<uintptr_t>(context) << std::dec << std::endl;
    }

    constexpr unsigned int kRequiredStableHits = 10;
    if (observedRenderTargetHits_ == kRequiredStableHits) {
        std::cout << "[UI] Render target stabilized after " << observedRenderTargetHits_ << " hits." << std::endl;
    }
    return observedRenderTargetHits_ >= kRequiredStableHits;
}

void UiHookManager::ResetObservedRenderTarget() {
    observedWindow_ = nullptr;
    observedRenderContext_ = nullptr;
    observedRenderTargetHits_ = 0;
}

bool UiHookManager::PerformRenderThreadShutdown(HGLRC currentContext) {
    if (renderThreadShutdownComplete_.load(std::memory_order_acquire)) {
        return true;
    }

    if (!menu_.IsInitialized()) {
        renderContext_ = nullptr;
        ResetObservedRenderTarget();
        renderThreadShutdownComplete_.store(true, std::memory_order_release);
        return true;
    }

    if (renderContext_ != nullptr && currentContext != renderContext_) {
        std::cout << "[UI] Shutdown observed context mismatch. Abandoning stale renderer context 0x" << std::hex
                  << reinterpret_cast<uintptr_t>(renderContext_) << " from current 0x"
                  << reinterpret_cast<uintptr_t>(currentContext) << std::dec << std::endl;
        menu_.AbandonRendererState(true);
        renderContext_ = nullptr;
        ResetObservedRenderTarget();
        renderThreadShutdownComplete_.store(true, std::memory_order_release);
        return true;
    }

    loggedRenderThreadShutdownWait_ = false;
    std::cout << "[UI] Performing render-thread shutdown on thread " << GetCurrentThreadId() << " window=0x"
              << std::hex << reinterpret_cast<uintptr_t>(window_) << " context=0x"
              << reinterpret_cast<uintptr_t>(currentContext) << std::dec << std::endl;
    menu_.ShutdownForCurrentContext(currentContext, true);
    renderContext_ = nullptr;
    ResetObservedRenderTarget();
    renderThreadShutdownComplete_.store(true, std::memory_order_release);
    std::cout << "[UI] Render-thread shutdown complete." << std::endl;
    return true;
}

void UiHookManager::ResetBoundRenderer(bool resetMenuVisibility) {
    if (!menu_.IsInitialized()) {
        renderContext_ = nullptr;
        ResetObservedRenderTarget();
        return;
    }

    const HGLRC currentContext = wglGetCurrentContext();
    if (renderContext_ != nullptr && currentContext == renderContext_) {
        menu_.ShutdownForCurrentContext(currentContext, resetMenuVisibility);
    } else {
        std::cout << "[UI] Rebinding renderer from stale context 0x" << std::hex
                  << reinterpret_cast<uintptr_t>(renderContext_) << " to 0x"
                  << reinterpret_cast<uintptr_t>(currentContext) << std::dec << std::endl;
        menu_.AbandonRendererState(resetMenuVisibility);
    }

    renderContext_ = nullptr;
    ResetObservedRenderTarget();
}

bool UiHookManager::WaitForRenderThreadShutdown() {
    if (!menu_.IsInitialized()) {
        renderThreadShutdownComplete_.store(true, std::memory_order_release);
        return true;
    }

    constexpr int kMaxWaitIterations = 500;
    for (int attempt = 0; attempt < kMaxWaitIterations; ++attempt) {
        if (renderThreadShutdownComplete_.load(std::memory_order_acquire)) {
            return true;
        }
        Sleep(10);
    }

    return false;
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

BOOL UiHookManager::RenderMenuAndCallOriginal(HDC hDc, SwapBuffersFn originalFn, const char *hookName) {
    if (originalFn == nullptr) {
        return FALSE;
    }

    CallbackScope callbackScope(activeSwapCalls_);

    if (inSwapHook_) {
        return originalFn(hDc);
    }
    inSwapHook_ = true;
    struct SwapHookReset {
        bool &flag;
        ~SwapHookReset() { flag = false; }
    } reset{inSwapHook_};

    std::scoped_lock renderLock(renderMutex_);

    if (!loggedFirstRenderCallback_) {
        std::cout << "[UI] Render callback entered for the first time via " << hookName << "." << std::endl;
        loggedFirstRenderCallback_ = true;
    }

    const HGLRC currentContext = wglGetCurrentContext();
    if (currentContext == nullptr) {
        if (!loggedMissingContext_) {
            std::cout << "[UI] Swap callback observed without an active OpenGL context." << std::endl;
            loggedMissingContext_ = true;
        }
        return originalFn(hDc);
    }
    loggedMissingContext_ = false;

    if (shuttingDown_) {
        PerformRenderThreadShutdown(currentContext);
        return originalFn(hDc);
    }

    if (HWND candidateWindow = ResolveGameWindow(hDc); candidateWindow != nullptr) {
        const bool isBoundRenderTarget = window_ == candidateWindow && renderContext_ == currentContext;
        if (!isBoundRenderTarget) {
            if (!ObserveStableRenderTarget(candidateWindow, currentContext)) {
                return originalFn(hDc);
            }

            if (menu_.IsInitialized()) {
                ResetBoundRenderer(false);
            }

            if (!AttachToWindow(candidateWindow)) {
                return originalFn(hDc);
            }

            renderContext_ = currentContext;
            ResetObservedRenderTarget();
        }

        if (window_ == candidateWindow && renderContext_ == currentContext) {
            if (!menu_.IsInitialized()) {
                if (!menu_.Initialize(window_, renderContext_)) {
                    std::cout << "[!] ERROR: Failed to initialize ImGui." << std::endl;
                    return originalFn(hDc);
                }
                std::cout << "[UI] ImGui initialized for window 0x" << std::hex
                          << reinterpret_cast<uintptr_t>(window_) << " context=0x"
                          << reinterpret_cast<uintptr_t>(renderContext_) << std::dec << std::endl;
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
    return instance_->RenderMenuAndCallOriginal(hDc, instance_->originalSwapBuffers_, "gdi32!SwapBuffers");
}

BOOL WINAPI UiHookManager::HookedWglSwapBuffers(HDC hDc) {
    if (instance_ == nullptr) {
        return FALSE;
    }
    return instance_->RenderMenuAndCallOriginal(hDc, instance_->originalWglSwapBuffers_, "opengl32!wglSwapBuffers");
}

LRESULT CALLBACK UiHookManager::HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    UiHookManager *instance = instance_;
    if (instance == nullptr) {
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    CallbackScope callbackScope(instance->activeWndProcCalls_);
    std::scoped_lock renderLock(instance->renderMutex_);

    if (!instance->shuttingDown_ && instance->menu_.HandleWndProc(hWnd, uMsg, wParam, lParam)) {
        return 0;
    }

    if (instance->originalWndProc_ == nullptr) {
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return CallWindowProc(instance->originalWndProc_, hWnd, uMsg, wParam, lParam);
}
