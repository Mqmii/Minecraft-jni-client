#include <iostream>
#include <windows.h>
#include <thread>

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

    UiHookManager uiHookManager;
    if (!uiHookManager.Initialize()) {
        std::cout << "[!] ERROR: Failed to initialize UI hooks." << std::endl;
        jniEnvironment.Shutdown();
        Sleep(1000);
        CleanupConsole(outputStream, inputStream);
        FreeLibraryAndExitThread(hModule, 0);
        return 0;
    }

    Minecraft mc;
    mc.fullBright();

    FastBreak fast_break(&mc);
    LocalPlayer player(&mc);
    EntityHitResult entity_hit_result(&mc);
    FastPlace fast_place;
    Velocity velocity;
    Entity entity(&mc);
    Esp esp;
    uiHookManager.SetEsp(&esp);

    while (uiHookManager.State().running && (GetAsyncKeyState(VK_DELETE) & 0x8000) == 0) {
        const ImGuiMenuState &menuState = uiHookManager.State();
        if (menuState.fastBreak) fast_break.break_fast();
        if (menuState.fastPlace) fast_place.onUpdate();
        if (menuState.sprint) player.Sprint();
        if (menuState.velocity) velocity.anti_knockback();
        if (menuState.triggerBot) entity_hit_result.isEntity();
        if (menuState.tracer && esp.IsInitialized()) esp.Tick();
        Sleep(20);
    }
    uiHookManager.Shutdown();
    jniEnvironment.Shutdown();

    Sleep(1000);
    CleanupConsole(outputStream, inputStream);
    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
