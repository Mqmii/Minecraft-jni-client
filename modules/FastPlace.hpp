#pragma once

#include <jni.h>

class FastPlace {
    jfieldID clickDelayfield{};

public:
    FastPlace();

    void onUpdate();
};
