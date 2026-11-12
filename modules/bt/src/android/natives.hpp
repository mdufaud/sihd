#ifndef __SIHD_BT_ANDROID_NATIVES_HPP__
#define __SIHD_BT_ANDROID_NATIVES_HPP__

#include <jni.h>

// handle: the native object listening
namespace sihd::bt::natives
{

void JNICALL scan_on_result(JNIEnv *env,
                            jclass,
                            jlong handle,
                            jstring address,
                            jint address_type,
                            jstring name,
                            jint rssi,
                            jboolean connectable,
                            jint class_of_device,
                            jbyteArray record);
void JNICALL scan_on_failure(JNIEnv *env, jclass, jlong handle, jint error_code);

void JNICALL gatt_on_connection(JNIEnv *env, jclass, jlong handle, jboolean connected, jint status);
void JNICALL gatt_on_complete(JNIEnv *env, jclass, jlong handle, jint status, jbyteArray value);
void JNICALL gatt_on_notification(JNIEnv *env, jclass, jlong handle, jint instance, jbyteArray value);

void JNICALL local_gatt_on_complete(JNIEnv *env, jclass, jlong handle, jint status);
jbyteArray JNICALL local_gatt_on_read(JNIEnv *env, jclass, jlong handle, jstring service, jstring characteristic);
jboolean JNICALL
    local_gatt_on_write(JNIEnv *env, jclass, jlong handle, jstring service, jstring characteristic, jbyteArray value);

void JNICALL advertising_on_complete(JNIEnv *env, jclass, jlong handle, jint status);

void JNICALL bond_on_state(JNIEnv *env, jclass, jlong handle, jint state);

} // namespace sihd::bt::natives

#endif
