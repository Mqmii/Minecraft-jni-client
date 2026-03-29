#include "JniEnvironment.hpp"

#include <cstdint>
#include <iostream>
#include <windows.h>

using GetCreatedJavaVMsFn = jint(JNICALL *)(JavaVM **, jsize, jsize *);

bool JniEnvironment::Initialize(const char *threadName) {
    if (attached_) {
        std::cout << "[JNI] Initialize called while already attached." << std::endl;
        return true;
    }

    HMODULE hJvmDll = GetModuleHandleW(L"jvm.dll");
    if (hJvmDll == nullptr) {
        std::cout << "[!] ERROR: jvm.dll not found." << std::endl;
        return false;
    }
    std::cout << "[JNI] jvm.dll handle resolved at 0x" << std::hex << reinterpret_cast<uintptr_t>(hJvmDll)
              << std::dec << std::endl;

    auto getCreatedJavaVMs = reinterpret_cast<GetCreatedJavaVMsFn>(
        GetProcAddress(hJvmDll, "JNI_GetCreatedJavaVMs"));
    if (getCreatedJavaVMs == nullptr) {
        std::cout << "[!] ERROR: Failed to resolve JNI_GetCreatedJavaVMs." << std::endl;
        return false;
    }
    std::cout << "[JNI] JNI_GetCreatedJavaVMs resolved." << std::endl;

    jsize vmCount = 0;
    const jint vmResult = getCreatedJavaVMs(&vm_, 1, &vmCount);
    std::cout << "[JNI] JNI_GetCreatedJavaVMs returned status=" << vmResult << " vmCount=" << vmCount << std::endl;
    if (vmResult != JNI_OK || vm_ == nullptr) {
        std::cout << "[!] ERROR: JavaVM not found." << std::endl;
        return false;
    }
    std::cout << "[JNI] Using JavaVM at 0x" << std::hex << reinterpret_cast<uintptr_t>(vm_) << std::dec << std::endl;

    JavaVMAttachArgs attachArgs{};
    attachArgs.version = JNI_VERSION_1_8;
    attachArgs.name = const_cast<char *>(threadName);
    attachArgs.group = nullptr;

    std::cout << "[JNI] Attaching current thread with name '" << threadName << "'." << std::endl;
    const jint attachStatus = vm_->AttachCurrentThread(reinterpret_cast<void **>(&env_), &attachArgs);
    if (attachStatus != JNI_OK) {
        env_ = nullptr;
        std::cout << "[!] ERROR: Failed to attach current thread to JVM. status=" << attachStatus << std::endl;
        return false;
    }

    attached_ = true;
    activeVm_ = vm_;
    std::cout << "[JNI] Thread attached. JNIEnv=0x" << std::hex << reinterpret_cast<uintptr_t>(env_) << std::dec
              << std::endl;
    return true;
}

void JniEnvironment::Shutdown() {
    if (attached_ && vm_ != nullptr) {
        std::cout << "[JNI] Detaching current thread from JVM." << std::endl;
        vm_->DetachCurrentThread();
    }

    if (activeVm_ == vm_) {
        activeVm_ = nullptr;
    }

    attached_ = false;
    env_ = nullptr;
    vm_ = nullptr;
}

JNIEnv *JniEnvironment::GetCurrentEnv() {
    if (activeVm_ == nullptr) {
        return nullptr;
    }

    JNIEnv *env = nullptr;
    const jint status = activeVm_->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_8);
    return status == JNI_OK ? env : nullptr;
}

JNIEnv *JniEnvironment::GetOrAttachCurrentEnv(const char *threadName) {
    if (activeVm_ == nullptr) {
        return nullptr;
    }

    JNIEnv *env = nullptr;
    const jint status = activeVm_->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_8);
    if (status == JNI_OK) {
        return env;
    }
    if (status != JNI_EDETACHED) {
        return nullptr;
    }

    JavaVMAttachArgs attachArgs{};
    attachArgs.version = JNI_VERSION_1_8;
    attachArgs.name = const_cast<char *>(threadName != nullptr ? threadName : "JNI Cheat Render");
    attachArgs.group = nullptr;

    const jint attachStatus = activeVm_->AttachCurrentThread(reinterpret_cast<void **>(&env), &attachArgs);
    if (attachStatus != JNI_OK) {
        return nullptr;
    }

    helperAttachedCurrentThread_ = true;
    std::cout << "[JNI] Lazily attached thread " << GetCurrentThreadId() << " as '"
              << (threadName != nullptr ? threadName : "JNI Cheat Render") << "'." << std::endl;
    return env;
}

void JniEnvironment::DetachCurrentThreadIfNeeded() {
    if (!helperAttachedCurrentThread_ || activeVm_ == nullptr) {
        return;
    }

    activeVm_->DetachCurrentThread();
    helperAttachedCurrentThread_ = false;
    std::cout << "[JNI] Detached lazily attached thread " << GetCurrentThreadId() << "." << std::endl;
}

JavaVM *JniEnvironment::GetCurrentVm() {
    return activeVm_;
}

JNIEnv *JniEnvironment::GetEnv() const {
    return env_;
}

JavaVM *JniEnvironment::GetVm() const {
    return vm_;
}

bool JniEnvironment::IsAttached() const {
    return attached_;
}
