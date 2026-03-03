
#ifndef MINECRAFTWRAPPERJNI_FASTPLACE_HPP
#define MINECRAFTWRAPPERJNI_FASTPLACE_HPP
#include <jni.h>

class FastPlace {
    JNIEnv* env;
    jfieldID clickDelayfield{};
public:
    FastPlace(JNIEnv* p_env);
    void onUPdate();
};

#endif
