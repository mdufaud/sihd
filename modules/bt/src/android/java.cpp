#include "java.hpp"

#include <array>
#include <mutex>
#include <optional>
#include <utility>

#include <sihd/bt/android/context.hpp>

#include "natives.hpp"

namespace sihd::bt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace
{

// filled once by set_context(): the java vm is one per process
struct Registry
{
        std::mutex mutex;
        std::optional<java::Runtime> runtime;
};

Registry & registry()
{
    static Registry instance;
    return instance;
}

struct ShimClass
{
        const char *name;
        jclass java::Classes::*slot;
        std::vector<JNINativeMethod> natives;
};

std::vector<ShimClass> shim_classes()
{
    return {
        {"sihd.bt.Adapters", &java::Classes::adapters, {}},
        {"sihd.bt.Scan",
         &java::Classes::scan,
         {
             {"nativeOnResult",
              "(JLjava/lang/String;ILjava/lang/String;IZI[B)V",
              reinterpret_cast<void *>(&natives::scan_on_result)},
             {"nativeOnScanFailed", "(JI)V", reinterpret_cast<void *>(&natives::scan_on_failure)},
         }},
        {"sihd.bt.Gatt",
         &java::Classes::gatt,
         {
             {"nativeOnConnection", "(JZI)V", reinterpret_cast<void *>(&natives::gatt_on_connection)},
             {"nativeOnComplete", "(JI[B)V", reinterpret_cast<void *>(&natives::gatt_on_complete)},
             {"nativeOnNotification", "(JI[B)V", reinterpret_cast<void *>(&natives::gatt_on_notification)},
         }},
        {"sihd.bt.LocalGatt",
         &java::Classes::local_gatt,
         {
             {"nativeOnComplete", "(JI)V", reinterpret_cast<void *>(&natives::local_gatt_on_complete)},
             {"nativeOnRead",
              "(JLjava/lang/String;Ljava/lang/String;)[B",
              reinterpret_cast<void *>(&natives::local_gatt_on_read)},
             {"nativeOnWrite",
              "(JLjava/lang/String;Ljava/lang/String;[B)Z",
              reinterpret_cast<void *>(&natives::local_gatt_on_write)},
         }},
        {"sihd.bt.Advertising",
         &java::Classes::advertising,
         {{"nativeOnComplete", "(JI)V", reinterpret_cast<void *>(&natives::advertising_on_complete)}}},
        {"sihd.bt.Stream", &java::Classes::stream, {}},
        {"sihd.bt.Stream$Listener", &java::Classes::listener, {}},
        {"sihd.bt.Bond",
         &java::Classes::bond,
         {{"nativeOnBond", "(JI)V", reinterpret_cast<void *>(&natives::bond_on_state)}}},
    };
}

// a native thread's FindClass only sees the system classes: the shim loads through the app class loader
Result<jclass> load_class(JNIEnv *env, jobject loader, const char *name)
{
    jclass loader_class = env->GetObjectClass(loader);
    jmethodID load = env->GetMethodID(loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    env->DeleteLocalRef(loader_class);
    jstring java_name = java::jstring_of(env, name);
    auto local = static_cast<jclass>(env->CallObjectMethod(loader, load, java_name));
    env->DeleteLocalRef(java_name);
    auto loaded = java::check(env, name);
    SIHD_UNEXPECTED_RETURN(loaded);
    auto global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return global;
}

void release(JNIEnv *env, const java::Classes & classes)
{
    for (const ShimClass & shim : shim_classes())
    {
        if (classes.*shim.slot != nullptr)
            env->DeleteGlobalRef(classes.*shim.slot);
    }
}

Result<jclass> register_class(JNIEnv *env, jobject loader, const ShimClass & shim)
{
    auto loaded = load_class(env, loader, shim.name);
    if (!loaded)
        return std::unexpected(Error(not_found, "{}: the java side of sihd::bt is missing", loaded.error().message));
    if (shim.natives.empty()
        || env->RegisterNatives(*loaded, shim.natives.data(), static_cast<jint>(shim.natives.size())) == JNI_OK)
        return *loaded;
    env->DeleteGlobalRef(*loaded);
    auto registered = java::check(env, shim.name);
    return std::unexpected(registered ? Error(io_error, "could not register the natives of {}", shim.name)
                                      : registered.error());
}

Result<java::Classes> load_classes(JNIEnv *env, jobject context)
{
    jclass context_class = env->GetObjectClass(context);
    jmethodID get_loader = env->GetMethodID(context_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
    env->DeleteLocalRef(context_class);
    jobject loader = env->CallObjectMethod(context, get_loader);
    auto got = java::check(env, "the class loader of the context");
    SIHD_UNEXPECTED_RETURN(got);

    java::Classes classes;
    Result<jclass> registered;
    for (const ShimClass & shim : shim_classes())
    {
        registered = register_class(env, loader, shim);
        if (!registered)
            break;
        classes.*shim.slot = *registered;
    }
    env->DeleteLocalRef(loader);
    if (!registered)
        release(env, classes);
    SIHD_UNEXPECTED_RETURN(registered);
    return classes;
}

// an activity context would keep the activity alive
Result<jobject> application_of(JNIEnv *env, jobject context)
{
    jclass context_class = env->GetObjectClass(context);
    jmethodID get_application = env->GetMethodID(context_class, "getApplicationContext", "()Landroid/content/Context;");
    env->DeleteLocalRef(context_class);
    jobject application = env->CallObjectMethod(context, get_application);
    auto got = java::check(env, "the application context");
    SIHD_UNEXPECTED_RETURN(got);
    return application;
}

std::string message_of(JNIEnv *env, jthrowable throwable)
{
    jclass throwable_class = env->FindClass("java/lang/Throwable");
    jmethodID to_string = env->GetMethodID(throwable_class, "toString", "()Ljava/lang/String;");
    env->DeleteLocalRef(throwable_class);
    auto text = static_cast<jstring>(env->CallObjectMethod(throwable, to_string));
    if (env->ExceptionCheck())
    {
        env->ExceptionClear();
        return "unreadable java exception";
    }
    std::string message = java::string(env, text);
    env->DeleteLocalRef(text);
    return message;
}

bool is_instance(JNIEnv *env, jthrowable throwable, const char *class_name)
{
    jclass exception_class = env->FindClass(class_name);
    const bool instance = exception_class != nullptr && env->IsInstanceOf(throwable, exception_class);
    env->DeleteLocalRef(exception_class);
    return instance;
}

} // namespace

namespace android_
{

Result<void> set_context(JavaVM *vm, jobject context)
{
    if (vm == nullptr || context == nullptr)
        return std::unexpected(Error(invalid_argument, "set_context needs a java vm and a context"));
    Registry & current = registry();
    std::lock_guard lock(current.mutex);
    // the java vm and its application context are one per process
    if (current.runtime.has_value())
        return {};
    java::Env env(vm);
    if (!env)
        return std::unexpected(java::detached());

    auto application = application_of(env.get(), context);
    SIHD_UNEXPECTED_RETURN(application);
    auto classes = load_classes(env.get(), *application);
    jobject global_context = classes ? env->NewGlobalRef(*application) : nullptr;
    env->DeleteLocalRef(*application);
    SIHD_UNEXPECTED_RETURN(classes);
    current.runtime = java::Runtime {.vm = vm, .context = global_context, .classes = *classes};
    return {};
}

} // namespace android_

namespace java
{

Result<Runtime> runtime()
{
    Registry & current = registry();
    std::lock_guard lock(current.mutex);
    if (!current.runtime.has_value())
        return std::unexpected(Error(not_initialized, "sihd::bt::android_::set_context() was not called"));
    return *current.runtime;
}

Env::Env(JavaVM *vm): _scoped(vm) {}

Env::operator bool() const
{
    return static_cast<bool>(_scoped);
}

JNIEnv *Env::operator->() const
{
    return _scoped.get();
}

JNIEnv *Env::get() const
{
    return _scoped.get();
}

Error detached()
{
    return Error(io_error, "could not attach the thread to the java vm");
}

Result<void> check(JNIEnv *env, std::string_view what)
{
    jthrowable throwable = env->ExceptionOccurred();
    if (throwable == nullptr)
        return {};
    env->ExceptionClear();
    ErrorCode code = io_error;
    if (is_instance(env, throwable, "java/lang/SecurityException"))
        code = permission_denied;
    else if (is_instance(env, throwable, "java/lang/IllegalArgumentException"))
        code = invalid_argument;
    else if (is_instance(env, throwable, "java/lang/UnsupportedOperationException"))
        code = not_supported;
    std::string message = message_of(env, throwable);
    env->DeleteLocalRef(throwable);
    return std::unexpected(Error(code, "{}: {}", what, message));
}

Object::Object(JNIEnv *env, jobject local)
{
    if (local == nullptr)
        return;
    env->GetJavaVM(&_vm);
    _object = env->NewGlobalRef(local);
    env->DeleteLocalRef(local);
}

Object::~Object()
{
    this->_release();
}

Object::Object(Object && other) noexcept:
    _vm(std::exchange(other._vm, nullptr)),
    _object(std::exchange(other._object, nullptr))
{
}

Object & Object::operator=(Object && other) noexcept
{
    if (this != &other)
    {
        this->_release();
        _vm = std::exchange(other._vm, nullptr);
        _object = std::exchange(other._object, nullptr);
    }
    return *this;
}

jobject Object::get() const
{
    return _object;
}

Object::operator bool() const
{
    return _object != nullptr;
}

void Object::_release()
{
    if (_object == nullptr)
        return;
    Env env(_vm);
    if (env)
        env->DeleteGlobalRef(_object);
    _object = nullptr;
}

std::vector<uint8_t> bytes(JNIEnv *env, jbyteArray array)
{
    if (array == nullptr)
        return {};
    std::vector<uint8_t> copied(static_cast<size_t>(env->GetArrayLength(array)));
    env->GetByteArrayRegion(array, 0, static_cast<jsize>(copied.size()), reinterpret_cast<jbyte *>(copied.data()));
    return copied;
}

jbyteArray byte_array(JNIEnv *env, ArrUByteView bytes)
{
    jbyteArray array = env->NewByteArray(static_cast<jsize>(bytes.size()));
    env->SetByteArrayRegion(array, 0, static_cast<jsize>(bytes.size()), reinterpret_cast<const jbyte *>(bytes.data()));
    return array;
}

std::vector<std::string> strings(JNIEnv *env, jobjectArray array)
{
    std::vector<std::string> copied;
    if (array == nullptr)
        return copied;
    const jsize length = env->GetArrayLength(array);
    copied.reserve(static_cast<size_t>(length));
    for (jsize i = 0; i < length; ++i)
    {
        auto element = static_cast<jstring>(env->GetObjectArrayElement(array, i));
        copied.emplace_back(string(env, element));
        env->DeleteLocalRef(element);
    }
    return copied;
}

jobjectArray string_array(JNIEnv *env, const std::vector<std::string> & strings)
{
    jclass string_class = env->FindClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(strings.size()), string_class, nullptr);
    env->DeleteLocalRef(string_class);
    for (size_t i = 0; i < strings.size(); ++i)
    {
        jstring element = jstring_of(env, strings[i]);
        env->SetObjectArrayElement(array, static_cast<jsize>(i), element);
        env->DeleteLocalRef(element);
    }
    return array;
}

std::string string(JNIEnv *env, jstring value)
{
    return sbt::jni::to_string(env, value);
}

jstring jstring_of(JNIEnv *env, std::string_view value)
{
    return sbt::jni::to_jstring(env, value);
}

void Completion::reset()
{
    auto lock = _waitable.guard();
    _answer.reset();
    _aborted = false;
}

void Completion::expect()
{
    auto lock = _waitable.guard();
    _answer.reset();
}

void Completion::complete(int status, std::vector<uint8_t> value)
{
    {
        auto lock = _waitable.guard();
        _answer = Answer {.status = status, .value = std::move(value)};
    }
    _waitable.notify_all();
}

void Completion::abort()
{
    {
        auto lock = _waitable.guard();
        _aborted = true;
    }
    _waitable.notify_all();
}

Result<Completion::Answer> Completion::wait(Duration timeout, std::string_view what)
{
    auto lock = _waitable.wait_for_guard(timeout, [this] { return _answer.has_value() || _aborted; });
    if (_answer.has_value())
        return std::move(*_answer);
    if (_aborted)
        return std::unexpected(Error(closed, "{}: the bluetooth link is gone", what));
    return std::unexpected(Error(ErrorCode::timeout, "{}: no answer", what));
}

} // namespace java

} // namespace sihd::bt
