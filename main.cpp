#include <iostream>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
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
HMODULE gInjectedModule = nullptr;

void CleanupConsole(FILE *outputStream, FILE *inputStream) {
    if (outputStream != nullptr) {
        fclose(outputStream);
    }
    if (inputStream != nullptr) {
        fclose(inputStream);
    }
    FreeConsole();
}

void AppendCrashLine(char *buffer, const size_t bufferSize, size_t &offset, const char *text) {
    if (buffer == nullptr || bufferSize == 0 || offset >= bufferSize) {
        return;
    }

    const int written = std::snprintf(buffer + offset, bufferSize - offset, "%s", text != nullptr ? text : "");
    if (written <= 0) {
        return;
    }

    offset += static_cast<size_t>(written);
    if (offset >= bufferSize) {
        offset = bufferSize - 1;
    }
}

void AppendCrashFormatted(char *buffer, const size_t bufferSize, size_t &offset, const char *format, ...) {
    if (buffer == nullptr || bufferSize == 0 || offset >= bufferSize || format == nullptr) {
        return;
    }

    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(buffer + offset, bufferSize - offset, format, args);
    va_end(args);
    if (written <= 0) {
        return;
    }

    offset += static_cast<size_t>(written);
    if (offset >= bufferSize) {
        offset = bufferSize - 1;
    }
}

void WriteCrashReport(const char *message) {
    const char *safeMessage = message != nullptr ? message : "[CRASH] No crash message available.\r\n";

    DWORD ignored = 0;
    const HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (consoleHandle != nullptr && consoleHandle != INVALID_HANDLE_VALUE) {
        WriteConsoleA(consoleHandle, safeMessage, static_cast<DWORD>(std::strlen(safeMessage)), &ignored, nullptr);
    }

    char tempPath[MAX_PATH]{};
    const DWORD tempPathLength = GetTempPathA(MAX_PATH, tempPath);
    if (tempPathLength == 0 || tempPathLength >= MAX_PATH) {
        return;
    }

    char crashPath[MAX_PATH]{};
    std::snprintf(crashPath, sizeof(crashPath), "%sMinecraftWrapperJNI_crash.log", tempPath);

    const HANDLE fileHandle = CreateFileA(
        crashPath,
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return;
    }

    WriteFile(fileHandle, safeMessage, static_cast<DWORD>(std::strlen(safeMessage)), &ignored, nullptr);
    FlushFileBuffers(fileHandle);
    CloseHandle(fileHandle);
}

void PauseOnCrash() {
    static bool alreadyPaused = false;
    if (alreadyPaused) {
        Sleep(INFINITE);
        return;
    }

    alreadyPaused = true;
    WriteCrashReport("[CRASH] Press Enter in the console to terminate the process.\r\n");

    const HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
    if (inputHandle == nullptr || inputHandle == INVALID_HANDLE_VALUE) {
        Sleep(INFINITE);
        return;
    }

    INPUT_RECORD inputRecord{};
    DWORD eventsRead = 0;
    while (ReadConsoleInputA(inputHandle, &inputRecord, 1, &eventsRead)) {
        if (inputRecord.EventType == KEY_EVENT &&
            inputRecord.Event.KeyEvent.bKeyDown &&
            inputRecord.Event.KeyEvent.wVirtualKeyCode == VK_RETURN) {
            break;
        }
    }
}

LONG WINAPI CrashExceptionFilter(EXCEPTION_POINTERS *exceptionInfo) {
    char crashBuffer[2048]{};
    size_t offset = 0;

    AppendCrashLine(crashBuffer, sizeof(crashBuffer), offset,
                    "\r\n[CRASH] Unhandled exception intercepted.\r\n");

    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);
    AppendCrashFormatted(crashBuffer, sizeof(crashBuffer), offset,
                         "[CRASH] Time: %04u-%02u-%02u %02u:%02u:%02u.%03u\r\n",
                         localTime.wYear, localTime.wMonth, localTime.wDay,
                         localTime.wHour, localTime.wMinute, localTime.wSecond, localTime.wMilliseconds);
    AppendCrashFormatted(crashBuffer, sizeof(crashBuffer), offset,
                         "[CRASH] Module: 0x%p ThreadId=%lu\r\n",
                         gInjectedModule, GetCurrentThreadId());

    if (exceptionInfo != nullptr &&
        exceptionInfo->ExceptionRecord != nullptr &&
        exceptionInfo->ContextRecord != nullptr) {
        const EXCEPTION_RECORD *record = exceptionInfo->ExceptionRecord;
        AppendCrashFormatted(crashBuffer, sizeof(crashBuffer), offset,
                             "[CRASH] Code: 0x%08lX Address: 0x%p Flags: 0x%08lX\r\n",
                             record->ExceptionCode, record->ExceptionAddress, record->ExceptionFlags);
#if defined(_M_X64)
        const CONTEXT *context = exceptionInfo->ContextRecord;
        AppendCrashFormatted(crashBuffer, sizeof(crashBuffer), offset,
                             "[CRASH] RIP=0x%llX RSP=0x%llX RBP=0x%llX\r\n",
                             static_cast<unsigned long long>(context->Rip),
                             static_cast<unsigned long long>(context->Rsp),
                             static_cast<unsigned long long>(context->Rbp));
#elif defined(_M_IX86)
        const CONTEXT *context = exceptionInfo->ContextRecord;
        AppendCrashFormatted(crashBuffer, sizeof(crashBuffer), offset,
                             "[CRASH] EIP=0x%08lX ESP=0x%08lX EBP=0x%08lX\r\n",
                             context->Eip, context->Esp, context->Ebp);
#endif
    } else {
        AppendCrashLine(crashBuffer, sizeof(crashBuffer), offset,
                        "[CRASH] Exception pointers unavailable.\r\n");
    }

    AppendCrashLine(crashBuffer, sizeof(crashBuffer), offset,
                    "[CRASH] A copy was written to %TEMP%\\MinecraftWrapperJNI_crash.log\r\n");
    WriteCrashReport(crashBuffer);
    PauseOnCrash();
    return EXCEPTION_EXECUTE_HANDLER;
}

void InstallCrashDiagnostics(HMODULE moduleHandle) {
    gInjectedModule = moduleHandle;
    SetUnhandledExceptionFilter(CrashExceptionFilter);
}
} // namespace

DWORD WINAPI MainThread(LPVOID moduleHandle) {
    HMODULE hModule = static_cast<HMODULE>(moduleHandle);
    AllocConsole();
    FILE *outputStream = nullptr;
    FILE *inputStream = nullptr;
    freopen_s(&outputStream, "CONOUT$", "w", stdout);
    freopen_s(&inputStream, "CONIN$", "r", stdin);
    InstallCrashDiagnostics(hModule);

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
        uiHookManager.SetEsp(&esp);

        std::cout << "[BOOT] Entering main feature loop." << std::endl;
        while ((GetAsyncKeyState(VK_DELETE) & 0x8000) == 0) {
            const ImGuiMenuState menuState = uiHookManager.GetStateSnapshot();
            if (!menuState.running) {
                break;
            }
            if (menuState.fastBreak) fast_break.break_fast();
            if (menuState.fastPlace) fast_place.onUpdate();
            if (menuState.sprint) player.Sprint();
            if (menuState.velocity) velocity.anti_knockback();
            if (menuState.triggerBot) entity_hit_result.isEntity();
            Sleep(20);
        }
        std::cout << "[BOOT] Main feature loop exited. Cleaning up UI." << std::endl;
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
