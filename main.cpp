#include <iostream>
#include <cstdint>
#include <cstdio>
#include <windows.h>

#include "CrashDiagnostics.hpp"
#include "modules/FastBreak.hpp"
#include "modules/hitResult.hpp"
#include "classes/minecraft.hpp"
#include "classes/Entity.hpp"
#include "modules/FastPlace.hpp"
#include "modules/Velocity.hpp"
#include "jni/JniEnvironment.hpp"
#include "ui/UiHookManager.hpp"
#include "modules/Esp.hpp"
namespace {
class RenderThreadGameplayController final : public GameplayController {
public:
    RenderThreadGameplayController(FastBreak &fastBreak, LocalPlayer &player, EntityHitResult &entityHitResult,
                                   FastPlace &fastPlace, Velocity &velocity)
        : fastBreak_(fastBreak),
          player_(player),
          entityHitResult_(entityHitResult),
          fastPlace_(fastPlace),
          velocity_(velocity) {
    }

    void OnRenderFrame(const ImGuiMenuState &state) override {
        if (!state.running) {
            return;
        }

        if (state.fastBreak) {
            fastBreak_.break_fast();
        }
        if (state.fastPlace) {
            fastPlace_.onUpdate();
        }
        if (state.sprint) {
            player_.Sprint();
        }
        if (state.velocity) {
            velocity_.anti_knockback();
        }
        if (state.triggerBot) {
            entityHitResult_.isEntity();
        }
    }

private:
    FastBreak &fastBreak_;
    LocalPlayer &player_;
    EntityHitResult &entityHitResult_;
    FastPlace &fastPlace_;
    Velocity &velocity_;
};

void CleanupConsole(FILE *outputStream, FILE *inputStream) {
    if (outputStream != nullptr) {
        fclose(outputStream);
    }
    if (inputStream != nullptr) {
        fclose(inputStream);
    }
    FreeConsole();
}
} // namespace

DWORD WINAPI MainThread(LPVOID moduleHandle) {
    HMODULE hModule = static_cast<HMODULE>(moduleHandle);
    AllocConsole();
    FILE *outputStream = nullptr;
    FILE *inputStream = nullptr;
    freopen_s(&outputStream, "CONOUT$", "w", stdout);
    freopen_s(&inputStream, "CONIN$", "r", stdin);
    CrashDiagnostics::Install(hModule);

    std::cout << "[BOOT] MainThread started for module 0x" << std::hex << reinterpret_cast<uintptr_t>(hModule)
              << std::dec << std::endl;
    std::cout << "[+] DLL injected." << std::endl;

    JniEnvironment jniEnvironment;
    if (!jniEnvironment.Initialize("JNI Cheat")) {
        Sleep(3000);
        CleanupConsole(outputStream, inputStream);
        FreeLibraryAndExitThread(hModule, 0);
        return 0;
    }

    std::cout << "[+] SUCCESS: Attached to JVM." << std::endl;
    std::cout << "[+] Press 'DELETE' to exit." << std::endl;

    {
        UiHookManager uiHookManager;
        if (!uiHookManager.Initialize()) {
            std::cout << "[!] ERROR: Failed to initialize UI hooks." << std::endl;
            jniEnvironment.Shutdown();
            Sleep(1000);
            CleanupConsole(outputStream, inputStream);
            FreeLibraryAndExitThread(hModule, 0);
            return 0;
        }

        std::cout << "[BOOT] Constructing gameplay modules." << std::endl;
        Minecraft mc;
        mc.fullBright();

        FastBreak fast_break(&mc);
        LocalPlayer player(&mc);
        EntityHitResult entity_hit_result(&mc);
        FastPlace fast_place;
        Velocity velocity;
        Entity entity(&mc);
        Esp esp;
        RenderThreadGameplayController gameplayController(fast_break, player, entity_hit_result, fast_place, velocity);
        uiHookManager.SetEsp(&esp);
        uiHookManager.SetGameplayController(&gameplayController);

        std::cout << "[BOOT] Entering main feature loop." << std::endl;
        uint64_t lastEspTick = 0;
        while ((GetAsyncKeyState(VK_DELETE) & 0x8000) == 0) {
            const ImGuiMenuState menuState = uiHookManager.GetStateSnapshot();
            if (!menuState.running) {
                break;
            }

            const bool espActive = menuState.tracer || menuState.boxEsp || menuState.espDebug;
            const uint64_t now = GetTickCount64();
            if (espActive && (lastEspTick == 0 || (now - lastEspTick) >= 16)) {
                esp.Tick();
                lastEspTick = now;
            }

            Sleep(10);
        }
        std::cout << "[BOOT] Main feature loop exited. Cleaning up UI." << std::endl;
        uiHookManager.SetGameplayController(nullptr);
        uiHookManager.SetEsp(nullptr);
        uiHookManager.Shutdown();
    }
    std::cout << "[BOOT] Gameplay objects destroyed. Detaching from JVM." << std::endl;
    jniEnvironment.Shutdown();

    Sleep(1000);
    CleanupConsole(outputStream, inputStream);
    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        OutputDebugStringA("[BOOT] DLL attach received.\n");
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
