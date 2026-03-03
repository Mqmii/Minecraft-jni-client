#include "JniEnvironment.hpp"

#include <iostream>
#include <windows.h>

using GetCreatedJavaVMsFn = jint(JNICALL *)(JavaVM **, jsize, jsize *);

bool JniEnvironment::Initialize(const char *threadName) {
    if (attached_) {
        return true;
    }

    HMODULE hJvmDll = GetModuleHandleW(L"jvm.dll");
    if (hJvmDll == nullptr) {
        std::cout << "[!] ERROR: jvm.dll not found." << std::endl;
        return false;
    }

    auto getCreatedJavaVMs = reinterpret_cast<GetCreatedJavaVMsFn>(
        GetProcAddress(hJvmDll, "JNI_GetCreatedJavaVMs"));
    if (getCreatedJavaVMs == nullptr) {
        std::cout << "[!] ERROR: Failed to resolve JNI_GetCreatedJavaVMs." << std::endl;
        return false;
    }

    jsize vmCount = 0;
    const jint vmResult = getCreatedJavaVMs(&vm_, 1, &vmCount);
    if (vmResult != JNI_OK || vm_ == nullptr) {
        std::cout << "[!] ERROR: JavaVM not found." << std::endl;
        return false;
    }

    JavaVMAttachArgs attachArgs{};
    attachArgs.version = JNI_VERSION_1_8;
    attachArgs.name = const_cast<char *>(threadName);
    attachArgs.group = nullptr;

    if (vm_->AttachCurrentThread(reinterpret_cast<void **>(&env_), &attachArgs) != JNI_OK) {
        env_ = nullptr;
        std::cout << "[!] ERROR: Failed to attach current thread to JVM." << std::endl;
        return false;
    }

    attached_ = true;
    return true;
}

void JniEnvironment::Shutdown() {
    if (attached_ && vm_ != nullptr) {
        vm_->DetachCurrentThread();
    }

    attached_ = false;
    env_ = nullptr;
    vm_ = nullptr;
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
