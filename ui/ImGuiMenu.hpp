#pragma once

#include <windows.h>

struct ImGuiMenuState {
    bool running = true;
    bool fastBreak = false;
    bool fastPlace = false;
    bool sprint = false;
    bool velocity = false;
    bool triggerBot = false;
};

class ImGuiMenu {
public:
    bool Initialize(HWND window);
    void Shutdown();
    void RenderFrame();
    bool HandleWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    bool IsInitialized() const;
    ImGuiMenuState &State();
    const ImGuiMenuState &State() const;

private:
    void ApplyMenuInputState();
    void UpdateToggleState();
    void DrawMenu();

    bool initialized_ = false;
    bool showMenu_ = false;
    bool menuInputEnabled_ = false;
    bool insertWasDown_ = false;
    int cursorVisibilityAdjustments_ = 0;
    HWND window_ = nullptr;
    ImGuiMenuState state_;
};
