#include "CrashDiagnostics.hpp"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <dbghelp.h>
#include <tlhelp32.h>

#include "jni/JniEnvironment.hpp"
#include "ui/UiHookManager.hpp"

namespace {
HMODULE gInjectedModule = nullptr;
constexpr bool kPauseOnCrash = false;
constexpr size_t kCrashBufferSize = 64 * 1024;
constexpr size_t kSymbolNameMaxLength = 512;
constexpr size_t kInstructionByteCount = 32;
constexpr size_t kStackByteCount = 128;
constexpr size_t kMaxStackFrames = 32;
constexpr size_t kMaxModuleEntries = 40;
bool gSymbolsInitialized = false;

const char *ExceptionCodeToString(const DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT: return "EXCEPTION_BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DENORMAL_OPERAND: return "EXCEPTION_FLT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT: return "EXCEPTION_FLT_INEXACT_RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW: return "EXCEPTION_FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK: return "EXCEPTION_FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW: return "EXCEPTION_FLT_UNDERFLOW";
        case EXCEPTION_GUARD_PAGE: return "EXCEPTION_GUARD_PAGE";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW: return "EXCEPTION_INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION: return "EXCEPTION_INVALID_DISPOSITION";
        case EXCEPTION_INVALID_HANDLE: return "EXCEPTION_INVALID_HANDLE";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_SINGLE_STEP: return "EXCEPTION_SINGLE_STEP";
        case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
        default: return "UNKNOWN_EXCEPTION";
    }
}

const char *MemoryStateToString(const DWORD state) {
    switch (state) {
        case MEM_COMMIT: return "MEM_COMMIT";
        case MEM_FREE: return "MEM_FREE";
        case MEM_RESERVE: return "MEM_RESERVE";
        default: return "MEM_UNKNOWN";
    }
}

const char *MemoryTypeToString(const DWORD type) {
    switch (type) {
        case MEM_IMAGE: return "MEM_IMAGE";
        case MEM_MAPPED: return "MEM_MAPPED";
        case MEM_PRIVATE: return "MEM_PRIVATE";
        default: return "MEM_UNKNOWN";
    }
}

const char *ProtectToString(const DWORD protect) {
    switch (protect & 0xFF) {
        case PAGE_EXECUTE: return "PAGE_EXECUTE";
        case PAGE_EXECUTE_READ: return "PAGE_EXECUTE_READ";
        case PAGE_EXECUTE_READWRITE: return "PAGE_EXECUTE_READWRITE";
        case PAGE_EXECUTE_WRITECOPY: return "PAGE_EXECUTE_WRITECOPY";
        case PAGE_NOACCESS: return "PAGE_NOACCESS";
        case PAGE_READONLY: return "PAGE_READONLY";
        case PAGE_READWRITE: return "PAGE_READWRITE";
        case PAGE_WRITECOPY: return "PAGE_WRITECOPY";
        default: return "PAGE_UNKNOWN";
    }
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

void AppendSectionHeader(char *buffer, const size_t bufferSize, size_t &offset, const char *title) {
    AppendCrashFormatted(buffer, bufferSize, offset, "\r\n[CRASH] ==== %s ====\r\n", title != nullptr ? title : "Unknown");
}

void InitializeSymbolEngine() {
    if (gSymbolsInitialized) {
        return;
    }

    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    gSymbolsInitialized = SymInitialize(process, nullptr, TRUE) == TRUE;
}

void AppendProcessInfo(char *buffer, const size_t bufferSize, size_t &offset) {
    AppendSectionHeader(buffer, bufferSize, offset, "Process");

    char imagePath[MAX_PATH]{};
    DWORD imagePathLength = static_cast<DWORD>(sizeof(imagePath));
    if (QueryFullProcessImageNameA(GetCurrentProcess(), 0, imagePath, &imagePathLength) == FALSE) {
        std::snprintf(imagePath, sizeof(imagePath), "<unknown>");
    }

    char currentDirectory[MAX_PATH]{};
    if (GetCurrentDirectoryA(MAX_PATH, currentDirectory) == 0) {
        std::snprintf(currentDirectory, sizeof(currentDirectory), "<unknown>");
    }

    SYSTEM_INFO systemInfo{};
    GetNativeSystemInfo(&systemInfo);

    BOOL isWow64 = FALSE;
    IsWow64Process(GetCurrentProcess(), &isWow64);

    AppendCrashFormatted(buffer, bufferSize, offset,
                         "[CRASH] Image=%s\r\n"
                         "[CRASH] WorkingDirectory=%s\r\n"
                         "[CRASH] CommandLine=%s\r\n"
                         "[CRASH] ProcessId=%lu ThreadId=%lu UptimeMs=%llu\r\n"
                         "[CRASH] Build=%s %s PointerSize=%zu ProcessorArch=%u ProcessorCount=%lu Wow64=%d\r\n",
                         imagePath,
                         currentDirectory,
                         GetCommandLineA(),
                         GetCurrentProcessId(),
                         GetCurrentThreadId(),
                         static_cast<unsigned long long>(GetTickCount64()),
                         __DATE__,
                         __TIME__,
                         sizeof(void *),
                         static_cast<unsigned>(systemInfo.wProcessorArchitecture),
                         systemInfo.dwNumberOfProcessors,
                         isWow64 ? 1 : 0);
}

void AppendJniState(char *buffer, const size_t bufferSize, size_t &offset) {
    AppendSectionHeader(buffer, bufferSize, offset, "JNI");
    JavaVM *vm = JniEnvironment::GetCurrentVm();
    JNIEnv *env = JniEnvironment::GetCurrentEnv();
    AppendCrashFormatted(buffer, bufferSize, offset,
                         "[CRASH] JavaVM=0x%p JNIEnv(CurrentThread)=0x%p Attached=%d\r\n",
                         vm,
                         env,
                         env != nullptr ? 1 : 0);
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
                         "[CRASH] FaultModule=%s Base=0x%p Offset=0x%llX\r\n",
                         modulePath,
                         faultModule,
                         static_cast<unsigned long long>(faultAddress - moduleBase));
}

void AppendExceptionParameters(char *buffer, const size_t bufferSize, size_t &offset, const EXCEPTION_RECORD *record) {
    if (record == nullptr) {
        return;
    }

    AppendCrashFormatted(buffer, bufferSize, offset,
                         "[CRASH] Exception=%s Code=0x%08lX Address=0x%p Flags=0x%08lX ParamCount=%lu\r\n",
                         ExceptionCodeToString(record->ExceptionCode),
                         record->ExceptionCode,
                         record->ExceptionAddress,
                         record->ExceptionFlags,
                         record->NumberParameters);

    for (DWORD parameterIndex = 0; parameterIndex < record->NumberParameters; ++parameterIndex) {
        AppendCrashFormatted(buffer, bufferSize, offset,
                             "[CRASH] ExceptionInfo[%lu]=0x%p\r\n",
                             parameterIndex,
                             reinterpret_cast<void *>(record->ExceptionInformation[parameterIndex]));
    }

    if ((record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
        record->NumberParameters >= 2) {
        const char *accessType = "unknown";
        switch (record->ExceptionInformation[0]) {
            case 0:
                accessType = "read";
                break;
            case 1:
                accessType = "write";
                break;
            case 8:
                accessType = "execute";
                break;
            default:
                break;
        }

        AppendCrashFormatted(buffer, bufferSize, offset,
                             "[CRASH] AccessViolation type=%s target=0x%p\r\n",
                             accessType,
                             reinterpret_cast<void *>(record->ExceptionInformation[1]));
        if (record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR && record->NumberParameters >= 3) {
            AppendCrashFormatted(buffer, bufferSize, offset,
                                 "[CRASH] InPageError NTSTATUS=0x%p\r\n",
                                 reinterpret_cast<void *>(record->ExceptionInformation[2]));
        }
    }

    if (record->ExceptionRecord != nullptr) {
        AppendCrashFormatted(buffer, bufferSize, offset,
                             "[CRASH] NestedExceptionRecord=0x%p\r\n",
                             record->ExceptionRecord);
    }
}

void AppendMemoryRegionInfo(char *buffer, const size_t bufferSize, size_t &offset, const void *address) {
    AppendSectionHeader(buffer, bufferSize, offset, "Memory");

    MEMORY_BASIC_INFORMATION memoryInfo{};
    if (address == nullptr || VirtualQuery(address, &memoryInfo, sizeof(memoryInfo)) == 0) {
        AppendCrashLine(buffer, bufferSize, offset, "[CRASH] VirtualQuery failed for fault address.\r\n");
        return;
    }

    AppendCrashFormatted(buffer, bufferSize, offset,
                         "[CRASH] Region Base=0x%p AllocationBase=0x%p RegionSize=0x%zX\r\n"
                         "[CRASH] State=%s Protect=%s AllocationProtect=%s Type=%s\r\n",
                         memoryInfo.BaseAddress,
                         memoryInfo.AllocationBase,
                         memoryInfo.RegionSize,
                         MemoryStateToString(memoryInfo.State),
                         ProtectToString(memoryInfo.Protect),
                         ProtectToString(memoryInfo.AllocationProtect),
                         MemoryTypeToString(memoryInfo.Type));
}

void AppendUiHookState(char *buffer, const size_t bufferSize, size_t &offset) {
    AppendSectionHeader(buffer, bufferSize, offset, "UI");

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
                         "[CRASH] renderThreadId=%lu window=0x%p context=0x%p features=0x%08X stage=%s insideSwap=%d menu=%d shutdown=%d\r\n",
                         snapshot.renderThreadId,
                         snapshot.window,
                         snapshot.renderContext,
                         snapshot.featureMask,
                         stageName,
                         snapshot.insideSwapHook ? 1 : 0,
                         snapshot.menuInitialized ? 1 : 0,
                         snapshot.shuttingDown ? 1 : 0);

    if (snapshot.window != nullptr && IsWindow(snapshot.window)) {
        char windowTitle[256]{};
        char className[256]{};
        GetWindowTextA(snapshot.window, windowTitle, static_cast<int>(sizeof(windowTitle)));
        GetClassNameA(snapshot.window, className, static_cast<int>(sizeof(className)));
        RECT clientRect{};
        GetClientRect(snapshot.window, &clientRect);
        AppendCrashFormatted(buffer, bufferSize, offset,
                             "[CRASH] WindowTitle=%s\r\n"
                             "[CRASH] WindowClass=%s ClientRect=%ld,%ld -> %ld,%ld\r\n",
                             windowTitle[0] != '\0' ? windowTitle : "<untitled>",
                             className[0] != '\0' ? className : "<unknown>",
                             clientRect.left,
                             clientRect.top,
                             clientRect.right,
                             clientRect.bottom);
    }
}

void AppendHexBytes(char *buffer, const size_t bufferSize, size_t &offset, const char *label,
                    const uintptr_t address, const size_t byteCount) {
    if (address == 0 || byteCount == 0) {
        return;
    }

    std::array<unsigned char, kStackByteCount> bytes{};
    SIZE_T bytesRead = 0;
    const HANDLE process = GetCurrentProcess();
    if (ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), bytes.data(),
                          std::min(bytes.size(), byteCount), &bytesRead) == FALSE || bytesRead == 0) {
        AppendCrashFormatted(buffer, bufferSize, offset,
                             "[CRASH] %s unavailable at 0x%p\r\n",
                             label,
                             reinterpret_cast<void *>(address));
        return;
    }

    AppendCrashFormatted(buffer, bufferSize, offset,
                         "[CRASH] %s @0x%p (%zu bytes)\r\n",
                         label,
                         reinterpret_cast<void *>(address),
                         static_cast<size_t>(bytesRead));

    for (size_t index = 0; index < bytesRead; index += 16) {
        AppendCrashFormatted(buffer, bufferSize, offset, "[CRASH]   0x%p: ",
                             reinterpret_cast<void *>(address + index));
        const size_t lineByteCount = std::min<size_t>(16, bytesRead - index);
        for (size_t byteIndex = 0; byteIndex < lineByteCount; ++byteIndex) {
            AppendCrashFormatted(buffer, bufferSize, offset, "%02X ", bytes[index + byteIndex]);
        }
        AppendCrashLine(buffer, bufferSize, offset, "\r\n");
    }
}

void AppendRegisters(char *buffer, const size_t bufferSize, size_t &offset, const CONTEXT *context) {
    if (context == nullptr) {
        return;
    }

    AppendSectionHeader(buffer, bufferSize, offset, "Registers");

#if defined(_M_X64)
    AppendCrashFormatted(buffer, bufferSize, offset,
                         "[CRASH] RIP=0x%016llX RSP=0x%016llX RBP=0x%016llX EFLAGS=0x%08lX\r\n"
                         "[CRASH] RAX=0x%016llX RBX=0x%016llX RCX=0x%016llX RDX=0x%016llX\r\n"
                         "[CRASH] RSI=0x%016llX RDI=0x%016llX R8 =0x%016llX R9 =0x%016llX\r\n"
                         "[CRASH] R10=0x%016llX R11=0x%016llX R12=0x%016llX R13=0x%016llX\r\n"
                         "[CRASH] R14=0x%016llX R15=0x%016llX\r\n",
                         static_cast<unsigned long long>(context->Rip),
                         static_cast<unsigned long long>(context->Rsp),
                         static_cast<unsigned long long>(context->Rbp),
                         context->EFlags,
                         static_cast<unsigned long long>(context->Rax),
                         static_cast<unsigned long long>(context->Rbx),
                         static_cast<unsigned long long>(context->Rcx),
                         static_cast<unsigned long long>(context->Rdx),
                         static_cast<unsigned long long>(context->Rsi),
                         static_cast<unsigned long long>(context->Rdi),
                         static_cast<unsigned long long>(context->R8),
                         static_cast<unsigned long long>(context->R9),
                         static_cast<unsigned long long>(context->R10),
                         static_cast<unsigned long long>(context->R11),
                         static_cast<unsigned long long>(context->R12),
                         static_cast<unsigned long long>(context->R13),
                         static_cast<unsigned long long>(context->R14),
                         static_cast<unsigned long long>(context->R15));
#elif defined(_M_IX86)
    AppendCrashFormatted(buffer, bufferSize, offset,
                         "[CRASH] EIP=0x%08lX ESP=0x%08lX EBP=0x%08lX EFLAGS=0x%08lX\r\n"
                         "[CRASH] EAX=0x%08lX EBX=0x%08lX ECX=0x%08lX EDX=0x%08lX\r\n"
                         "[CRASH] ESI=0x%08lX EDI=0x%08lX\r\n",
                         context->Eip,
                         context->Esp,
                         context->Ebp,
                         context->EFlags,
                         context->Eax,
                         context->Ebx,
                         context->Ecx,
                         context->Edx,
                         context->Esi,
                         context->Edi);
#endif
}

void AppendInstructionAndStackBytes(char *buffer, const size_t bufferSize, size_t &offset, const CONTEXT *context) {
    if (context == nullptr) {
        return;
    }

    AppendSectionHeader(buffer, bufferSize, offset, "Bytes");

#if defined(_M_X64)
    AppendHexBytes(buffer, bufferSize, offset, "InstructionBytes", static_cast<uintptr_t>(context->Rip),
                   kInstructionByteCount);
    AppendHexBytes(buffer, bufferSize, offset, "StackBytes", static_cast<uintptr_t>(context->Rsp), kStackByteCount);
#elif defined(_M_IX86)
    AppendHexBytes(buffer, bufferSize, offset, "InstructionBytes", static_cast<uintptr_t>(context->Eip),
                   kInstructionByteCount);
    AppendHexBytes(buffer, bufferSize, offset, "StackBytes", static_cast<uintptr_t>(context->Esp), kStackByteCount);
#endif
}

void AppendStackTrace(char *buffer, const size_t bufferSize, size_t &offset, const CONTEXT *context) {
    AppendSectionHeader(buffer, bufferSize, offset, "StackTrace");

    if (context == nullptr) {
        AppendCrashLine(buffer, bufferSize, offset, "[CRASH] Stack trace unavailable: missing context.\r\n");
        return;
    }

    InitializeSymbolEngine();
    if (!gSymbolsInitialized) {
        AppendCrashLine(buffer, bufferSize, offset, "[CRASH] Stack trace unavailable: SymInitialize failed.\r\n");
        return;
    }

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    CONTEXT traceContext = *context;
    STACKFRAME64 frame{};
    DWORD machineType = 0;

#if defined(_M_X64)
    machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = traceContext.Rip;
    frame.AddrFrame.Offset = traceContext.Rbp;
    frame.AddrStack.Offset = traceContext.Rsp;
#elif defined(_M_IX86)
    machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = traceContext.Eip;
    frame.AddrFrame.Offset = traceContext.Ebp;
    frame.AddrStack.Offset = traceContext.Esp;
#else
    AppendCrashLine(buffer, bufferSize, offset, "[CRASH] Stack trace unavailable on this architecture.\r\n");
    return;
#endif

    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    std::array<unsigned char, sizeof(SYMBOL_INFO) + kSymbolNameMaxLength> symbolStorage{};
    auto *symbol = reinterpret_cast<SYMBOL_INFO *>(symbolStorage.data());
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = kSymbolNameMaxLength - 1;

    bool emittedAnyFrame = false;
    for (size_t frameIndex = 0; frameIndex < kMaxStackFrames; ++frameIndex) {
        const BOOL walked = StackWalk64(machineType, process, thread, &frame, &traceContext, nullptr,
                                        SymFunctionTableAccess64, SymGetModuleBase64, nullptr);
        if (walked == FALSE || frame.AddrPC.Offset == 0) {
            break;
        }

        emittedAnyFrame = true;
        DWORD64 displacement = 0;
        IMAGEHLP_LINE64 lineInfo{};
        lineInfo.SizeOfStruct = sizeof(lineInfo);
        DWORD lineDisplacement = 0;
        const BOOL hasSymbol = SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol) == TRUE;
        const BOOL hasLine = SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &lineInfo) == TRUE;

        char modulePath[MAX_PATH]{};
        if (HMODULE frameModule = reinterpret_cast<HMODULE>(SymGetModuleBase64(process, frame.AddrPC.Offset));
            frameModule != nullptr) {
            GetModuleFileNameA(frameModule, modulePath, MAX_PATH);
        }

        AppendCrashFormatted(buffer, bufferSize, offset,
                             "[CRASH] #%02zu PC=0x%016llX Frame=0x%016llX Stack=0x%016llX %s+0x%llX\r\n",
                             frameIndex,
                             static_cast<unsigned long long>(frame.AddrPC.Offset),
                             static_cast<unsigned long long>(frame.AddrFrame.Offset),
                             static_cast<unsigned long long>(frame.AddrStack.Offset),
                             hasSymbol ? symbol->Name : "<unknown>",
                             static_cast<unsigned long long>(displacement));

        if (modulePath[0] != '\0') {
            AppendCrashFormatted(buffer, bufferSize, offset, "[CRASH]      Module=%s\r\n", modulePath);
        }
        if (hasLine) {
            AppendCrashFormatted(buffer, bufferSize, offset, "[CRASH]      Source=%s:%lu (+0x%lX)\r\n",
                                 lineInfo.FileName, lineInfo.LineNumber, lineDisplacement);
        }
    }

    if (!emittedAnyFrame) {
        AppendCrashLine(buffer, bufferSize, offset, "[CRASH] Stack walk produced no frames.\r\n");
    }
}

void AppendLoadedModules(char *buffer, const size_t bufferSize, size_t &offset) {
    AppendSectionHeader(buffer, bufferSize, offset, "Modules");

    HANDLE snapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshotHandle == INVALID_HANDLE_VALUE) {
        AppendCrashLine(buffer, bufferSize, offset, "[CRASH] CreateToolhelp32Snapshot failed.\r\n");
        return;
    }

    MODULEENTRY32 moduleEntry{};
    moduleEntry.dwSize = sizeof(moduleEntry);
    size_t emittedModuleCount = 0;
    if (Module32First(snapshotHandle, &moduleEntry) == TRUE) {
        do {
            AppendCrashFormatted(buffer, bufferSize, offset,
                                 "[CRASH] Module[%02zu] Base=0x%p Size=0x%08lX Name=%s Path=%s\r\n",
                                 emittedModuleCount,
                                 moduleEntry.modBaseAddr,
                                 moduleEntry.modBaseSize,
                                 moduleEntry.szModule,
                                 moduleEntry.szExePath);
            ++emittedModuleCount;
        } while (emittedModuleCount < kMaxModuleEntries && Module32Next(snapshotHandle, &moduleEntry) == TRUE);
    }

    if (emittedModuleCount == 0) {
        AppendCrashLine(buffer, bufferSize, offset, "[CRASH] No module entries captured.\r\n");
    }

    CloseHandle(snapshotHandle);
}

void BuildCrashArtifactPaths(char *logPath, size_t logPathSize, char *dumpPath, size_t dumpPathSize) {
    if (logPath == nullptr || logPathSize == 0 || dumpPath == nullptr || dumpPathSize == 0) {
        return;
    }

    char tempPath[MAX_PATH]{};
    const DWORD tempPathLength = GetTempPathA(MAX_PATH, tempPath);
    if (tempPathLength == 0 || tempPathLength >= MAX_PATH) {
        std::snprintf(logPath, logPathSize, "MinecraftWrapperJNI_crash.log");
        std::snprintf(dumpPath, dumpPathSize, "MinecraftWrapperJNI_crash.dmp");
        return;
    }

    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);
    const DWORD processId = GetCurrentProcessId();
    const DWORD threadId = GetCurrentThreadId();
    std::snprintf(logPath, logPathSize,
                  "%sMinecraftWrapperJNI_crash_%04u%02u%02u_%02u%02u%02u_%lu_%lu.log",
                  tempPath,
                  localTime.wYear,
                  localTime.wMonth,
                  localTime.wDay,
                  localTime.wHour,
                  localTime.wMinute,
                  localTime.wSecond,
                  processId,
                  threadId);
    std::snprintf(dumpPath, dumpPathSize,
                  "%sMinecraftWrapperJNI_crash_%04u%02u%02u_%02u%02u%02u_%lu_%lu.dmp",
                  tempPath,
                  localTime.wYear,
                  localTime.wMonth,
                  localTime.wDay,
                  localTime.wHour,
                  localTime.wMinute,
                  localTime.wSecond,
                  processId,
                  threadId);
}

void WriteTextFile(const char *path, const char *message) {
    if (path == nullptr || message == nullptr) {
        return;
    }

    DWORD ignored = 0;
    const HANDLE fileHandle = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return;
    }

    WriteFile(fileHandle, message, static_cast<DWORD>(std::strlen(message)), &ignored, nullptr);
    FlushFileBuffers(fileHandle);
    CloseHandle(fileHandle);
}

bool WriteMiniDumpFile(const char *path, EXCEPTION_POINTERS *exceptionInfo, DWORD *errorCode) {
    if (errorCode != nullptr) {
        *errorCode = 0;
    }

    if (path == nullptr || exceptionInfo == nullptr) {
        if (errorCode != nullptr) {
            *errorCode = ERROR_INVALID_PARAMETER;
        }
        return false;
    }

    const HANDLE dumpHandle = CreateFileA(
        path,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (dumpHandle == INVALID_HANDLE_VALUE) {
        if (errorCode != nullptr) {
            *errorCode = GetLastError();
        }
        return false;
    }

    MINIDUMP_EXCEPTION_INFORMATION exceptionDetails{};
    exceptionDetails.ThreadId = GetCurrentThreadId();
    exceptionDetails.ExceptionPointers = exceptionInfo;
    exceptionDetails.ClientPointers = FALSE;

    const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithThreadInfo |
        MiniDumpWithIndirectlyReferencedMemory |
        MiniDumpScanMemory |
        MiniDumpWithDataSegs |
        MiniDumpWithHandleData |
        MiniDumpWithUnloadedModules);

    const BOOL dumpResult = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        dumpHandle,
        dumpType,
        &exceptionDetails,
        nullptr,
        nullptr);
    const DWORD lastError = dumpResult == TRUE ? ERROR_SUCCESS : GetLastError();
    CloseHandle(dumpHandle);
    if (errorCode != nullptr) {
        *errorCode = lastError;
    }
    return dumpResult == TRUE;
}

void WriteCrashReportToConsole(const char *message) {
    if (message == nullptr) {
        return;
    }

    DWORD ignored = 0;
    const HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (consoleHandle != nullptr && consoleHandle != INVALID_HANDLE_VALUE) {
        WriteConsoleA(consoleHandle, message, static_cast<DWORD>(std::strlen(message)), &ignored, nullptr);
    }
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
    WriteCrashReportToConsole("[CRASH] Press Enter in the console to terminate the process.\r\n");

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
    std::array<char, kCrashBufferSize> crashBuffer{};
    size_t offset = 0;

    AppendCrashLine(crashBuffer.data(), crashBuffer.size(), offset,
                    "\r\n[CRASH] Unhandled exception intercepted.\r\n");

    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);
    AppendCrashFormatted(crashBuffer.data(), crashBuffer.size(), offset,
                         "[CRASH] Time=%04u-%02u-%02u %02u:%02u:%02u.%03u\r\n",
                         localTime.wYear, localTime.wMonth, localTime.wDay,
                         localTime.wHour, localTime.wMinute, localTime.wSecond, localTime.wMilliseconds);
    AppendCrashFormatted(crashBuffer.data(), crashBuffer.size(), offset,
                         "[CRASH] InjectedModule=0x%p LastError=%lu\r\n",
                         gInjectedModule, GetLastError());

    AppendProcessInfo(crashBuffer.data(), crashBuffer.size(), offset);
    AppendJniState(crashBuffer.data(), crashBuffer.size(), offset);
    AppendUiHookState(crashBuffer.data(), crashBuffer.size(), offset);

    if (exceptionInfo != nullptr &&
        exceptionInfo->ExceptionRecord != nullptr &&
        exceptionInfo->ContextRecord != nullptr) {
        const EXCEPTION_RECORD *record = exceptionInfo->ExceptionRecord;
        const CONTEXT *context = exceptionInfo->ContextRecord;

        AppendSectionHeader(crashBuffer.data(), crashBuffer.size(), offset, "Exception");
        AppendExceptionParameters(crashBuffer.data(), crashBuffer.size(), offset, record);
        AppendFaultingModuleInfo(crashBuffer.data(), crashBuffer.size(), offset, record->ExceptionAddress);
        AppendMemoryRegionInfo(crashBuffer.data(), crashBuffer.size(), offset, record->ExceptionAddress);
        AppendRegisters(crashBuffer.data(), crashBuffer.size(), offset, context);
        AppendInstructionAndStackBytes(crashBuffer.data(), crashBuffer.size(), offset, context);
        AppendStackTrace(crashBuffer.data(), crashBuffer.size(), offset, context);
    } else {
        AppendCrashLine(crashBuffer.data(), crashBuffer.size(), offset,
                        "[CRASH] Exception pointers unavailable.\r\n");
    }

    AppendLoadedModules(crashBuffer.data(), crashBuffer.size(), offset);

    char logPath[MAX_PATH]{};
    char dumpPath[MAX_PATH]{};
    BuildCrashArtifactPaths(logPath, sizeof(logPath), dumpPath, sizeof(dumpPath));

    DWORD dumpError = 0;
    const bool dumpWritten = WriteMiniDumpFile(dumpPath, exceptionInfo, &dumpError);
    AppendSectionHeader(crashBuffer.data(), crashBuffer.size(), offset, "Artifacts");
    AppendCrashFormatted(crashBuffer.data(), crashBuffer.size(), offset,
                         "[CRASH] TextLog=%s\r\n"
                         "[CRASH] MiniDump=%s (%s) LastError=%lu\r\n",
                         logPath,
                         dumpPath,
                         dumpWritten ? "written" : "failed",
                         dumpError);

    WriteCrashReportToConsole(crashBuffer.data());
    WriteTextFile(logPath, crashBuffer.data());
    PauseOnCrash();
    return EXCEPTION_EXECUTE_HANDLER;
}
} // namespace

namespace CrashDiagnostics {
void Install(HMODULE moduleHandle) {
    gInjectedModule = moduleHandle;
    InitializeSymbolEngine();
    SetUnhandledExceptionFilter(CrashExceptionFilter);
}
}
