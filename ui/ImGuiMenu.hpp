#pragma once

#include <windows.h>

class Esp;

struct ImGuiMenuState {
    bool running = true;
    bool fastBreak = false;
    bool fastPlace = false;
    bool sprint = false;
    bool velocity = false;
    bool triggerBot = false;
    bool tracer = false;
    bool boxEsp = false;
    bool espDebug = false;
    float tracerColor[4]{0.95f, 0.96f, 1.0f, 0.88f};
    float boxColor[4]{0.92f, 0.94f, 0.99f, 0.84f};
    float tracerThickness = 1.5f;
    float boxThickness = 1.35f;
};

class ImGuiMenu {
public:
    bool Initialize(HWND window);
    void Shutdown(bool shutdownRenderer = true);
    void RenderFrame();
    bool HandleWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void SetEsp(Esp *esp);

    bool IsInitialized() const;
    ImGuiMenuState &State();
    const ImGuiMenuState &State() const;

private:
    void ApplyMenuInputState();
    void UpdateToggleState();
    void UpdateTriggerBotHotkeyState();
    void ToggleMenu();
    void DrawMenu();
    void DrawTriggerBotHotkeyControl();
    bool TryBindTriggerBotHotkey(UINT uMsg, WPARAM wParam);

    bool initialized_ = false;
    bool showMenu_ = false;
    bool menuInputEnabled_ = false;
    bool insertWasDown_ = false;
    bool triggerBotHotkeyWasDown_ = false;
    bool waitingForTriggerBotHotkey_ = false;
    int cursorVisibilityAdjustments_ = 0;
    int triggerBotHotkey_ = VK_LMENU;
    HWND window_ = nullptr;
    Esp *esp_ = nullptr;
    ImGuiMenuState state_;
};
