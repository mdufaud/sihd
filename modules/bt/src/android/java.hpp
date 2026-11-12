#ifndef __SIHD_BT_ANDROID_JAVA_HPP__
#define __SIHD_BT_ANDROID_JAVA_HPP__

#include <jni.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Waitable.hpp>

#include <sbt/jni.hpp>

namespace sihd::bt::java
{

// the classes of modules/bt/java, global references kept for the process
struct Classes
{
        jclass adapters = nullptr;
        jclass scan = nullptr;
        jclass gatt = nullptr;
        jclass local_gatt = nullptr;
        jclass advertising = nullptr;
        jclass stream = nullptr;
        jclass listener = nullptr;
        jclass bond = nullptr;
};

struct Runtime
{
        JavaVM *vm = nullptr;
        jobject context = nullptr;
        Classes classes;
};

// not_initialized until android_::set_context() ran
sihd::util::Result<Runtime> runtime();

class Env
{
    public:
        explicit Env(JavaVM *vm);

        explicit operator bool() const;
        JNIEnv *operator->() const;
        JNIEnv *get() const;

    private:
        sbt::jni::ScopedEnv _scoped;
};

sihd::util::Error detached();

// clears the pending exception
sihd::util::Result<void> check(JNIEnv *env, std::string_view what);

class Object
{
    public:
        Object() = default;
        Object(JNIEnv *env, jobject local);
        ~Object();

        Object(Object && other) noexcept;
        Object & operator=(Object && other) noexcept;
        Object(const Object &) = delete;
        Object & operator=(const Object &) = delete;

        jobject get() const;
        explicit operator bool() const;

    private:
        void _release();

        JavaVM *_vm = nullptr;
        jobject _object = nullptr;
};

std::vector<uint8_t> bytes(JNIEnv *env, jbyteArray array);
jbyteArray byte_array(JNIEnv *env, sihd::util::ArrUByteView bytes);
std::vector<std::string> strings(JNIEnv *env, jobjectArray array);
jobjectArray string_array(JNIEnv *env, const std::vector<std::string> & strings);
std::string string(JNIEnv *env, jstring value);
jstring jstring_of(JNIEnv *env, std::string_view value);

class Completion
{
    public:
        struct Answer
        {
                int status = 0;
                std::vector<uint8_t> value;
        };

        // for a new link: no answer, not aborted
        void reset();
        // before starting the operation
        void expect();
        // from a java callback
        void complete(int status, std::vector<uint8_t> value);
        // ends a wait with a closed error: the link is gone
        void abort();

        // a timeout or closed error when no answer came
        sihd::util::Result<Answer> wait(sihd::util::Duration timeout, std::string_view what);

    private:
        sihd::util::Waitable _waitable;
        std::optional<Answer> _answer;
        bool _aborted = false;
};

} // namespace sihd::bt::java

#endif
