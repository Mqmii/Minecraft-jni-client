#pragma once

#include <mutex>
#include <windows.h>

class Esp;
class GameplayController;
struct ImGuiContext;

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
    bool Initialize(HWND window, HGLRC renderContext);
    bool ShutdownForCurrentContext(HGLRC currentContext, bool resetMenuVisibility = true);
    void AbandonRendererState(bool resetMenuVisibility = false);
    ImGuiMenuState AdvanceFrame(GameplayController *gameplayController);
    void RenderVisibleMenu();
    bool HandleWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void SetEsp(Esp *esp);
    void SetToggleKeyPollingEnabled(bool enabled);
    void SetRunning(bool running);

    bool IsInitialized() const;
    bool IsMenuVisible() const;
    HGLRC RenderContext() const;
    ImGuiMenuState GetStateSnapshot() const;

private:
    void ResetRuntimeState(bool resetMenuVisibility);
    void BindContext() const;
    void ApplyMenuInputState();
    void UpdateToggleState();
    void UpdateTriggerBotHotkeyState();
    void ToggleMenu(const char *source);
    void DrawModuleStatusOverlay() const;
    void DrawMenu();
    void DrawTriggerBotHotkeyControl();
    void DrawEspDebugSection() const;
    bool TryBindTriggerBotHotkey(UINT uMsg, WPARAM wParam);

    bool initialized_ = false;
    bool showMenu_ = false;
    bool menuInputEnabled_ = false;
    bool insertWasDown_ = false;
    bool loggedFirstRenderFrame_ = false;
    bool loggedFirstVisibleMenuFrame_ = false;
    bool toggleKeyPollingEnabled_ = true;
    bool triggerBotHotkeyWasDown_ = false;
    bool waitingForTriggerBotHotkey_ = false;
    int cursorVisibilityAdjustments_ = 0;
    int triggerBotHotkey_ = VK_LMENU;
    HWND window_ = nullptr;
    HGLRC renderContext_ = nullptr;
    ImGuiContext *context_ = nullptr;
    Esp *esp_ = nullptr;
    mutable std::mutex stateMutex_;
    ImGuiMenuState state_;
};
