#include "ImGuiMenu.hpp"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool ImGuiMenu::Initialize(HWND window) {
    if (initialized_) {
        return true;
    }

    ImGui::CreateContext();

    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(window)) {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init()) {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    window_ = window;
    showMenu_ = false;
    menuInputEnabled_ = false;
    insertWasDown_ = false;
    initialized_ = true;
    return true;
}

void ImGuiMenu::Shutdown(bool shutdownRenderer) {
    if (!initialized_) {
        return;
    }

    showMenu_ = false;
    ApplyMenuInputState();

    if (shutdownRenderer) {
        ImGui_ImplOpenGL3_Shutdown();
    } else {
        // We may be unloading from a non-render thread without the original GL context.
        // Clear renderer backend bookkeeping so DestroyContext() doesn't assert.
        ImGuiIO &io = ImGui::GetIO();
        ImGuiPlatformIO &platformIo = ImGui::GetPlatformIO();
        io.BackendRendererName = nullptr;
        io.BackendRendererUserData = nullptr;
        io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
        platformIo.ClearRendererHandlers();
    }
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    initialized_ = false;
    insertWasDown_ = false;
    window_ = nullptr;
}

void ImGuiMenu::UpdateToggleState() {
    const bool insertDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (insertDown && !insertWasDown_) {
        ToggleMenu();
    }
    insertWasDown_ = insertDown;
}

void ImGuiMenu::ToggleMenu() {
    showMenu_ = !showMenu_;
    ApplyMenuInputState();
}

void ImGuiMenu::ApplyMenuInputState() {
    if (!initialized_ || window_ == nullptr) {
        cursorVisibilityAdjustments_ = 0;
        menuInputEnabled_ = false;
        return;
    }

    if (showMenu_ == menuInputEnabled_) {
        return;
    }

    ImGui::GetIO().MouseDrawCursor = showMenu_;

    if (showMenu_) {
        CURSORINFO cursorInfo{};
        cursorInfo.cbSize = sizeof(cursorInfo);
        if (GetCursorInfo(&cursorInfo) != FALSE && (cursorInfo.flags & CURSOR_SHOWING) == 0) {
            while (ShowCursor(TRUE) < 0) {
                ++cursorVisibilityAdjustments_;
            }
            ++cursorVisibilityAdjustments_;
        }
        ClipCursor(nullptr);
        ReleaseCapture();
        SetCapture(nullptr);
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    } else {
        while (cursorVisibilityAdjustments_ > 0) {
            ShowCursor(FALSE);
            --cursorVisibilityAdjustments_;
        }
    }

    menuInputEnabled_ = showMenu_;
}

void ImGuiMenu::DrawMenu() {
    ImGui::Begin("JNI Cheat");

    ImGui::Text("Menu");
    ImGui::Separator();

    ImGui::Checkbox("Fast Break", &state_.fastBreak);
    ImGui::Checkbox("Fast Place", &state_.fastPlace);
    ImGui::Checkbox("Auto Sprint", &state_.sprint);
    ImGui::Checkbox("Anti knockback", &state_.velocity);
    ImGui::Checkbox("TriggerBot", &state_.triggerBot);

    ImGui::End();
}

void ImGuiMenu::RenderFrame() {
    if (!initialized_) {
        return;
    }

    UpdateToggleState();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (showMenu_) {
        DrawMenu();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool ImGuiMenu::HandleWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!initialized_) {
        return false;
    }

    if ((uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) && wParam == VK_INSERT) {
        ToggleMenu();
        insertWasDown_ = true;
        return true;
    }

    if ((uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP) && wParam == VK_INSERT) {
        insertWasDown_ = false;
        return showMenu_;
    }

    if (!showMenu_) {
        return false;
    }

    if (uMsg == WM_SETCURSOR) {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return true;
    }

    ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

    switch (uMsg) {
        case WM_INPUT:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_MOUSEMOVE:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_CHAR:
            return true;
        default:
            return false;
    }
}

bool ImGuiMenu::IsInitialized() const {
    return initialized_;
}

ImGuiMenuState &ImGuiMenu::State() {
    return state_;
}

const ImGuiMenuState &ImGuiMenu::State() const {
    return state_;
}
