#pragma once

#include <jni.h>

class JniEnvironment {
public:
    bool Initialize(const char *threadName = "JNI Cheat");
    void Shutdown();

    [[nodiscard]] static JNIEnv *GetCurrentEnv();
    [[nodiscard]] static JNIEnv *GetOrAttachCurrentEnv(const char *threadName = "JNI Cheat Render");
    static void DetachCurrentThreadIfNeeded();
    [[nodiscard]] static JavaVM *GetCurrentVm();
    [[nodiscard]] JNIEnv *GetEnv() const;
    [[nodiscard]] JavaVM *GetVm() const;
    [[nodiscard]] bool IsAttached() const;

private:
    inline static JavaVM *activeVm_ = nullptr;
    inline static thread_local bool helperAttachedCurrentThread_ = false;
    JavaVM *vm_ = nullptr;
    JNIEnv *env_ = nullptr;
    bool attached_ = false;
};
