#pragma once

#include <jni.h>

class JniEnvironment {
public:
    bool Initialize(const char *threadName = "JNI Cheat");
    void Shutdown();

    [[nodiscard]] JNIEnv *GetEnv() const;
    [[nodiscard]] JavaVM *GetVm() const;
    [[nodiscard]] bool IsAttached() const;

private:
    JavaVM *vm_ = nullptr;
    JNIEnv *env_ = nullptr;
    bool attached_ = false;
};
