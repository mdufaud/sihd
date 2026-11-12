#include <jni.h>

#include <sihd/bt/android/context.hpp>

#include <sbt/jni.hpp>

extern "C" JNIEXPORT jstring JNICALL Java_sihd_bt_demo_BtActivity_nativeSetContext(JNIEnv *env, jclass, jobject context)
{
    JavaVM *vm = nullptr;
    if (env->GetJavaVM(&vm) != JNI_OK)
        return sbt::jni::to_jstring(env, "no java vm");
    auto set = sihd::bt::android_::set_context(vm, context);
    return sbt::jni::to_jstring(env, set ? "" : set.error().message);
}
