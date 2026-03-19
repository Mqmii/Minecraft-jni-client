#pragma once

#include <jni.h>

class FastPlace {
    JNIEnv *env{};
    jfieldID clickDelayfield{};

public:
    FastPlace();

    void onUpdate();
};
