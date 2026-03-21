#include "ImGuiMenu.hpp"

#include <cfloat>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

#include "../modules/Esp.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
std::string VirtualKeyToString(int virtualKey) {
    switch (virtualKey) {
        case 0: return "None";
        case VK_INSERT: return "INSERT";
        case VK_DELETE: return "DELETE";
        case VK_HOME: return "HOME";
        case VK_END: return "END";
        case VK_PRIOR: return "PAGE_UP";
        case VK_NEXT: return "PAGE_DOWN";
        case VK_LEFT: return "LEFT";
        case VK_RIGHT: return "RIGHT";
        case VK_UP: return "UP";
        case VK_DOWN: return "DOWN";
        case VK_SPACE: return "SPACE";
        case VK_TAB: return "TAB";
        case VK_RETURN: return "ENTER";
        case VK_ESCAPE: return "ESC";
        case VK_BACK: return "BACKSPACE";
        case VK_CAPITAL: return "CAPS_LOCK";
        case VK_LSHIFT: return "LEFT_SHIFT";
        case VK_RSHIFT: return "RIGHT_SHIFT";
        case VK_LCONTROL: return "LEFT_CTRL";
        case VK_RCONTROL: return "RIGHT_CTRL";
        case VK_LMENU: return "LEFT_ALT";
        case VK_RMENU: return "RIGHT_ALT";
        default:
            break;
    }

    if ((virtualKey >= '0' && virtualKey <= '9') || (virtualKey >= 'A' && virtualKey <= 'Z')) {
        return std::string(1, static_cast<char>(virtualKey));
    }

    if (virtualKey >= VK_F1 && virtualKey <= VK_F24) {
        return "F" + std::to_string(virtualKey - VK_F1 + 1);
    }

    UINT scanCode = MapVirtualKeyA(static_cast<UINT>(virtualKey), MAPVK_VK_TO_VSC);
    LONG lParam = static_cast<LONG>(scanCode) << 16;
    switch (virtualKey) {
        case VK_RMENU:
        case VK_RCONTROL:
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_NUMLOCK:
        case VK_DIVIDE:
            lParam |= 1 << 24;
            break;
        default:
            break;
    }

    char keyName[64]{};
    if (GetKeyNameTextA(lParam, keyName, sizeof(keyName)) > 0) {
        return keyName;
    }

    std::ostringstream stream;
    stream << "VK_" << std::hex << std::uppercase << virtualKey;
    return stream.str();
}
} // namespace

bool ImGuiMenu::Initialize(HWND window, HGLRC renderContext) {
    if (initialized_) {
        return true;
    }

    std::cout << "[MENU] Creating ImGui context for window 0x" << std::hex << reinterpret_cast<uintptr_t>(window)
              << " context=0x" << reinterpret_cast<uintptr_t>(renderContext) << std::dec << std::endl;

    context_ = ImGui::CreateContext();
    renderContext_ = renderContext;
    BindContext();

    ImGui::StyleColorsDark();

    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    io.Fonts->AddFontDefault();

    if (!ImGui_ImplWin32_InitForOpenGL(window)) {
        ImGui::DestroyContext(context_);
        ResetRuntimeState(true);
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 150")) {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(context_);
        ResetRuntimeState(true);
        return false;
    }

    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    if (!ImGui_ImplOpenGL3_CreateDeviceObjects()) {
        std::cout << "[MENU] Failed to create OpenGL device objects." << std::endl;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(context_);
        ResetRuntimeState(true);
        return false;
    }
    std::cout << "[MENU] Font atlas and OpenGL device objects created successfully." << std::endl;

    window_ = window;
    menuInputEnabled_ = false;
    insertWasDown_ = false;
    toggleKeyPollingEnabled_ = true;
    triggerBotHotkeyWasDown_ = false;
    waitingForTriggerBotHotkey_ = false;
    triggerBotHotkey_ = VK_LMENU;
    loggedFirstRenderFrame_ = false;
    loggedFirstVisibleMenuFrame_ = false;
    initialized_ = true;
    ApplyMenuInputState();
    std::cout << "[MENU] ImGuiMenu initialization complete." << std::endl;
    return true;
}

bool ImGuiMenu::ShutdownForCurrentContext(HGLRC currentContext, bool resetMenuVisibility) {
    if (!initialized_) {
        ResetRuntimeState(resetMenuVisibility);
        return true;
    }

    if (renderContext_ != nullptr && currentContext != nullptr && currentContext != renderContext_) {
        return false;
    }

    BindContext();
    std::cout << "[MENU] Shutdown requested on thread " << GetCurrentThreadId() << " for context 0x" << std::hex
              << reinterpret_cast<uintptr_t>(currentContext != nullptr ? currentContext : renderContext_) << std::dec
              << std::endl;

    if (resetMenuVisibility) {
        showMenu_ = false;
    }
    ApplyMenuInputState();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(context_);
    ResetRuntimeState(resetMenuVisibility);
    std::cout << "[MENU] Shutdown complete." << std::endl;
    return true;
}

void ImGuiMenu::AbandonRendererState(bool resetMenuVisibility) {
    if (!initialized_) {
        ResetRuntimeState(resetMenuVisibility);
        return;
    }

    BindContext();
    std::cout << "[MENU] Abandoning renderer state for stale context 0x" << std::hex
              << reinterpret_cast<uintptr_t>(renderContext_) << std::dec << std::endl;

    if (resetMenuVisibility) {
        showMenu_ = false;
    }
    ApplyMenuInputState();
    ImGui_ImplWin32_Shutdown();

    ImGuiIO &io = ImGui::GetIO();
    ImGuiPlatformIO &platformIo = ImGui::GetPlatformIO();
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
    platformIo.ClearRendererHandlers();

    ImGui::DestroyContext(context_);
    ResetRuntimeState(resetMenuVisibility);
    std::cout << "[MENU] Renderer state abandoned." << std::endl;
}

void ImGuiMenu::ResetRuntimeState(bool resetMenuVisibility) {
    initialized_ = false;
    menuInputEnabled_ = false;
    insertWasDown_ = false;
    toggleKeyPollingEnabled_ = true;
    triggerBotHotkeyWasDown_ = false;
    waitingForTriggerBotHotkey_ = false;
    window_ = nullptr;
    renderContext_ = nullptr;
    context_ = nullptr;
    loggedFirstRenderFrame_ = false;
    loggedFirstVisibleMenuFrame_ = false;
    cursorVisibilityAdjustments_ = 0;
    if (resetMenuVisibility) {
        showMenu_ = false;
    }
}

void ImGuiMenu::BindContext() const {
    if (context_ != nullptr) {
        ImGui::SetCurrentContext(context_);
    }
}

void ImGuiMenu::UpdateToggleState() {
    if (toggleKeyPollingEnabled_) {
        const bool insertDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (insertDown && !insertWasDown_) {
            ToggleMenu("poll");
        }
        insertWasDown_ = insertDown;
    }

    UpdateTriggerBotHotkeyState();
}

void ImGuiMenu::UpdateTriggerBotHotkeyState() {
    if (waitingForTriggerBotHotkey_ || triggerBotHotkey_ == 0) {
        triggerBotHotkeyWasDown_ = false;
        return;
    }

    const bool hotkeyDown = (GetAsyncKeyState(triggerBotHotkey_) & 0x8000) != 0;
    if (hotkeyDown && !triggerBotHotkeyWasDown_) {
        std::scoped_lock stateLock(stateMutex_);
        state_.triggerBot = !state_.triggerBot;
    }
    triggerBotHotkeyWasDown_ = hotkeyDown;
}

void ImGuiMenu::ToggleMenu(const char *source) {
    showMenu_ = !showMenu_;
    if (!showMenu_) {
        loggedFirstVisibleMenuFrame_ = false;
    }
    std::cout << "[MENU] INSERT toggle (" << source << ") -> " << (showMenu_ ? "visible" : "hidden")
              << std::endl;
    ApplyMenuInputState();
}

void ImGuiMenu::ApplyMenuInputState() {
    if (!initialized_ || window_ == nullptr) {
        cursorVisibilityAdjustments_ = 0;
        menuInputEnabled_ = false;
        return;
    }

    BindContext();

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
    BindContext();
    std::scoped_lock stateLock(stateMutex_);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("JNI Cheat");

    ImGui::Text("Menu");
    ImGui::Separator();

    ImGui::Checkbox("Fast Break", &state_.fastBreak);
    ImGui::Checkbox("Fast Place", &state_.fastPlace);
    ImGui::Checkbox("Auto Sprint", &state_.sprint);
    ImGui::Checkbox("Anti knockback", &state_.velocity);
    ImGui::Checkbox("TriggerBot", &state_.triggerBot);
    ImGui::SameLine();
    DrawTriggerBotHotkeyControl();
    ImGui::Checkbox("Tracer", &state_.tracer);
    ImGui::Checkbox("Box ESP", &state_.boxEsp);
    ImGui::Checkbox("ESP Debug", &state_.espDebug);

    if (state_.tracer || state_.boxEsp || state_.espDebug) {
        ImGui::Separator();
    }

    if (state_.tracer) {
        ImGui::ColorEdit4("Tracer Color", state_.tracerColor, ImGuiColorEditFlags_AlphaBar);
        ImGui::SliderFloat("Tracer Thickness", &state_.tracerThickness, 0.5f, 6.0f, "%.2f");
    }

    if (state_.boxEsp) {
        ImGui::ColorEdit4("Box Color", state_.boxColor, ImGuiColorEditFlags_AlphaBar);
        ImGui::SliderFloat("Box Thickness", &state_.boxThickness, 0.5f, 6.0f, "%.2f");
    }

    ImGui::End();
}

void ImGuiMenu::DrawModuleStatusOverlay() const {
    BindContext();
    ImDrawList *drawList = ImGui::GetBackgroundDrawList();
    if (drawList == nullptr) {
        return;
    }

    const bool triggerBotEnabled = [&]() {
        std::scoped_lock stateLock(stateMutex_);
        return state_.triggerBot;
    }();

    const ImVec2 basePosition(14.0f, 14.0f);
    const float fontSize = ImGui::GetFontSize() + 5.0f;
    const char *statusText = triggerBotEnabled ? "TriggerBot: ON" : "TriggerBot: OFF";
    const ImU32 statusColor = triggerBotEnabled ? IM_COL32(150, 255, 170, 235) : IM_COL32(255, 170, 170, 235);
    const ImU32 shadowColor = IM_COL32(0, 0, 0, 185);

    drawList->AddText(nullptr, fontSize, ImVec2(basePosition.x + 1.0f, basePosition.y + 1.0f), shadowColor, statusText);
    drawList->AddText(nullptr, fontSize, basePosition, statusColor, statusText);
}

void ImGuiMenu::DrawTriggerBotHotkeyControl() {
    BindContext();
    ImGui::TextDisabled("Hotkey:");
    ImGui::SameLine();

    const std::string buttonLabel = waitingForTriggerBotHotkey_
                                        ? "Press key..."
                                        : VirtualKeyToString(triggerBotHotkey_);
    if (ImGui::Button(buttonLabel.c_str(), ImVec2(120.0f, 0.0f))) {
        waitingForTriggerBotHotkey_ = true;
        triggerBotHotkeyWasDown_ = false;
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click and press a key to bind TriggerBot. Press ESC to clear.");
    }
}

void ImGuiMenu::RenderFrame() {
    if (!initialized_) {
        return;
    }

    BindContext();
    if (!loggedFirstRenderFrame_) {
        std::cout << "[MENU] First RenderFrame call received." << std::endl;
        loggedFirstRenderFrame_ = true;
    }

    UpdateToggleState();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    DrawModuleStatusOverlay();

    const ImGuiMenuState stateSnapshot = GetStateSnapshot();
    if (esp_ != nullptr && (stateSnapshot.tracer || stateSnapshot.boxEsp || stateSnapshot.espDebug)) {
        esp_->Tick();
        esp_->RenderOverlay(stateSnapshot.tracer, stateSnapshot.boxEsp, stateSnapshot.espDebug,
                            stateSnapshot.tracerColor, stateSnapshot.tracerThickness, stateSnapshot.boxColor,
                            stateSnapshot.boxThickness);
    }

    if (showMenu_) {
        DrawMenu();
    }

    ImGui::Render();
    ImDrawData *drawData = ImGui::GetDrawData();
    if (showMenu_ && drawData != nullptr && !loggedFirstVisibleMenuFrame_) {
        loggedFirstVisibleMenuFrame_ = true;
        const unsigned long long fontTextureId = static_cast<unsigned long long>(ImGui::GetIO().Fonts->TexRef.GetTexID());
        std::cout << "[MENU] First visible frame: cmdLists=" << drawData->CmdListsCount
                  << " vtx=" << drawData->TotalVtxCount << " idx=" << drawData->TotalIdxCount << " fontTex=0x"
                  << std::hex << fontTextureId << std::dec << std::endl;
    }
    if (drawData != nullptr && drawData->CmdListsCount > 0) {
        ImGui_ImplOpenGL3_RenderDrawData(drawData);
    }
}

bool ImGuiMenu::HandleWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!initialized_) {
        return false;
    }

    BindContext();
    if (TryBindTriggerBotHotkey(uMsg, wParam)) {
        return true;
    }

    const bool isInitialKeyPress = (lParam & (1LL << 30)) == 0;
    if ((uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) && wParam == VK_INSERT && isInitialKeyPress) {
        ToggleMenu("wndproc");
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

bool ImGuiMenu::TryBindTriggerBotHotkey(UINT uMsg, WPARAM wParam) {
    if (!waitingForTriggerBotHotkey_) {
        return false;
    }

    if (uMsg != WM_KEYDOWN && uMsg != WM_SYSKEYDOWN) {
        return false;
    }

    if (wParam == VK_ESCAPE) {
        triggerBotHotkey_ = 0;
        waitingForTriggerBotHotkey_ = false;
        triggerBotHotkeyWasDown_ = false;
        return true;
    }

    triggerBotHotkey_ = static_cast<int>(wParam);
    waitingForTriggerBotHotkey_ = false;
    triggerBotHotkeyWasDown_ = true;
    return true;
}

void ImGuiMenu::SetEsp(Esp *esp) {
    esp_ = esp;
}

void ImGuiMenu::SetToggleKeyPollingEnabled(bool enabled) {
    toggleKeyPollingEnabled_ = enabled;
    insertWasDown_ = false;
}

void ImGuiMenu::SetRunning(bool running) {
    std::scoped_lock stateLock(stateMutex_);
    state_.running = running;
}

bool ImGuiMenu::IsInitialized() const {
    return initialized_;
}

HGLRC ImGuiMenu::RenderContext() const {
    return renderContext_;
}

ImGuiMenuState ImGuiMenu::GetStateSnapshot() const {
    std::scoped_lock stateLock(stateMutex_);
    return state_;
}
