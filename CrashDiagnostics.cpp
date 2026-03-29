#include "CrashDiagnostics.hpp"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "ui/UiHookManager.hpp"

namespace {
HMODULE gInjectedModule = nullptr;
constexpr bool kPauseOnCrash = false;

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

void AppendFaultingModuleInfo(char *buffer, const size_t bufferSize, size_t &offset, const void *address) {
    if (address == nullptr) {
        AppendCrashLine(buffer, bufferSize, offset, "[CRASH] Fault module unavailable.\r\n");
        return;
    }

    HMODULE faultModule = nullptr;
    const BOOL resolved = GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        static_cast<LPCSTR>(address),
        &faultModule);
    if (resolved == FALSE || faultModule == nullptr) {
        AppendCrashLine(buffer, bufferSize, offset, "[CRASH] Fault module unavailable.\r\n");
        return;
    }

    char modulePath[MAX_PATH]{};
    if (GetModuleFileNameA(faultModule, modulePath, MAX_PATH) == 0) {
        std::snprintf(modulePath, sizeof(modulePath), "<unknown>");
    }

    const auto moduleBase = reinterpret_cast<uintptr_t>(faultModule);
    const auto faultAddress = reinterpret_cast<uintptr_t>(address);
    AppendCrashFormatted(buffer, bufferSize, offset,
                         "[CRASH] FaultModule: %s Base=0x%p Offset=0x%llX\r\n",
                         modulePath,
                         faultModule,
                         static_cast<unsigned long long>(faultAddress - moduleBase));
}

void AppendUiHookState(char *buffer, const size_t bufferSize, size_t &offset) {
    const UiHookDebugSnapshot snapshot = UiHookManager::GetDebugSnapshot();
    const char *stageName = "idle";
    switch (snapshot.renderStage) {
        case UiHookRenderStage::FrameStart:
            stageName = "frame_start";
            break;
        case UiHookRenderStage::EspRender:
            stageName = "esp_render";
            break;
        case UiHookRenderStage::MenuRender:
            stageName = "menu_render";
            break;
        case UiHookRenderStage::FrameEnd:
            stageName = "frame_end";
            break;
        case UiHookRenderStage::Idle:
        default:
            break;
    }

    AppendCrashFormatted(buffer, bufferSize, offset,
                         "[CRASH] UI: renderThreadId=%lu window=0x%p context=0x%p features=0x%08X stage=%s insideSwap=%d menu=%d shutdown=%d\r\n",
                         snapshot.renderThreadId,
                         snapshot.window,
                         snapshot.renderContext,
                         snapshot.featureMask,
                         stageName,
                         snapshot.insideSwapHook ? 1 : 0,
                         snapshot.menuInitialized ? 1 : 0,
                         snapshot.shuttingDown ? 1 : 0);
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
    if (!kPauseOnCrash) {
        return;
    }

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
        AppendFaultingModuleInfo(crashBuffer, sizeof(crashBuffer), offset, record->ExceptionAddress);
        AppendUiHookState(crashBuffer, sizeof(crashBuffer), offset);
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
} // namespace

namespace CrashDiagnostics {
void Install(HMODULE moduleHandle) {
    gInjectedModule = moduleHandle;
    SetUnhandledExceptionFilter(CrashExceptionFilter);
}
}
