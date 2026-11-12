#include <atomic>
#include <functional>

#include <sihd/bt/Socket.hpp>
#include <sihd/util/str.hpp>

#include "java.hpp"

namespace sihd::bt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace
{

Result<java::Object> create(jclass java_class,
                            const char *name,
                            const char *signature,
                            std::string_view what,
                            const std::function<jobject(JNIEnv *, jclass, jmethodID, jobject)> & call)
{
    auto runtime = java::runtime();
    SIHD_UNEXPECTED_RETURN(runtime);
    java::Env env(runtime->vm);
    if (!env)
        return std::unexpected(java::detached());
    jmethodID factory = env->GetStaticMethodID(java_class, name, signature);
    jobject local = call(env.get(), java_class, factory, runtime->context);
    auto created = java::check(env.get(), what);
    SIHD_UNEXPECTED_RETURN(created);
    return java::Object(env.get(), local);
}

Result<jclass> shim_class(jclass java::Classes::*slot)
{
    auto runtime = java::runtime();
    SIHD_UNEXPECTED_RETURN(runtime);
    return runtime->classes.*slot;
}

} // namespace

struct Socket::Impl
{
        java::Object stream;
        java::Object listener;
        uint16_t psm = 0;
        bool listening = false;
        std::atomic<bool> open = true;
};

Socket::Socket() = default;

Socket::Socket(Socket && other) = default;

Socket & Socket::operator=(Socket && other) = default;

Socket::~Socket()
{
    this->close();
}

Result<void> Socket::connect_rfcomm(const std::string & address, const std::string & uuid)
{
    auto closed_socket = this->_check_closed();
    SIHD_UNEXPECTED_RETURN(closed_socket);
    auto stream_class = shim_class(&java::Classes::stream);
    SIHD_UNEXPECTED_RETURN(stream_class);
    auto stream = create(
        *stream_class,
        "rfcomm",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Lsihd/bt/Stream;",
        "RFCOMM connection",
        [&](JNIEnv *env, jclass java_class, jmethodID factory, jobject context) {
            jstring java_address = java::jstring_of(env, str::to_upper_copy(address));
            jstring java_uuid = java::jstring_of(env, uuid);
            jobject created = env->CallStaticObjectMethod(java_class, factory, context, java_address, java_uuid);
            env->DeleteLocalRef(java_address);
            env->DeleteLocalRef(java_uuid);
            return created;
        });
    SIHD_UNEXPECTED_RETURN(stream);

    auto impl = std::make_unique<Impl>();
    impl->stream = std::move(*stream);
    _impl = std::move(impl);
    _address = str::to_upper_copy(address);
    return {};
}

Result<void> Socket::connect_rfcomm_channel(const std::string &, uint8_t)
{
    return std::unexpected(Error(not_supported, "android connects RFCOMM through a service uuid only"));
}

Result<void> Socket::connect_l2cap(const std::string & address, AddressType type, uint16_t psm)
{
    auto closed_socket = this->_check_closed();
    SIHD_UNEXPECTED_RETURN(closed_socket);
    if (type == AddressType::unspecified)
        return std::unexpected(Error(invalid_argument, "an L2CAP LE channel needs the address type of {}", address));
    auto stream_class = shim_class(&java::Classes::stream);
    SIHD_UNEXPECTED_RETURN(stream_class);
    auto stream = create(*stream_class,
                         "l2cap",
                         "(Landroid/content/Context;Ljava/lang/String;I)Lsihd/bt/Stream;",
                         "L2CAP connection",
                         [&](JNIEnv *env, jclass java_class, jmethodID factory, jobject context) {
                             jstring java_address = java::jstring_of(env, str::to_upper_copy(address));
                             jobject created = env->CallStaticObjectMethod(java_class,
                                                                           factory,
                                                                           context,
                                                                           java_address,
                                                                           static_cast<jint>(psm));
                             env->DeleteLocalRef(java_address);
                             return created;
                         });
    SIHD_UNEXPECTED_RETURN(stream);

    auto impl = std::make_unique<Impl>();
    impl->stream = std::move(*stream);
    impl->psm = psm;
    _impl = std::move(impl);
    _address = str::to_upper_copy(address);
    return {};
}

Result<void> Socket::listen_rfcomm(const std::string & uuid, const std::string & name)
{
    auto closed_socket = this->_check_closed();
    SIHD_UNEXPECTED_RETURN(closed_socket);
    auto listener_class = shim_class(&java::Classes::listener);
    SIHD_UNEXPECTED_RETURN(listener_class);
    auto listener = create(
        *listener_class,
        "rfcomm",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Lsihd/bt/Stream$Listener;",
        "RFCOMM listener",
        [&](JNIEnv *env, jclass java_class, jmethodID factory, jobject context) {
            jstring java_uuid = java::jstring_of(env, uuid);
            jstring java_name = java::jstring_of(env, name);
            jobject created = env->CallStaticObjectMethod(java_class, factory, context, java_uuid, java_name);
            env->DeleteLocalRef(java_uuid);
            env->DeleteLocalRef(java_name);
            return created;
        });
    SIHD_UNEXPECTED_RETURN(listener);

    auto impl = std::make_unique<Impl>();
    impl->listener = std::move(*listener);
    impl->listening = true;
    _impl = std::move(impl);
    _address.clear();
    return {};
}

Result<void> Socket::listen_l2cap(uint16_t psm)
{
    auto closed_socket = this->_check_closed();
    SIHD_UNEXPECTED_RETURN(closed_socket);
    if (psm != 0)
        return std::unexpected(Error(not_supported, "android picks the L2CAP psm: listen on psm 0"));
    auto listener_class = shim_class(&java::Classes::listener);
    SIHD_UNEXPECTED_RETURN(listener_class);
    auto listener = create(*listener_class,
                           "l2cap",
                           "(Landroid/content/Context;)Lsihd/bt/Stream$Listener;",
                           "L2CAP listener",
                           [](JNIEnv *env, jclass java_class, jmethodID factory, jobject context) {
                               return env->CallStaticObjectMethod(java_class, factory, context);
                           });
    SIHD_UNEXPECTED_RETURN(listener);

    auto runtime = java::runtime();
    SIHD_UNEXPECTED_RETURN(runtime);
    java::Env env(runtime->vm);
    if (!env)
        return std::unexpected(java::detached());
    auto impl = std::make_unique<Impl>();
    impl->psm = static_cast<uint16_t>(
        env->CallIntMethod(listener->get(), env->GetMethodID(runtime->classes.listener, "psm", "()I")));
    auto read = java::check(env.get(), "L2CAP psm");
    SIHD_UNEXPECTED_RETURN(read);
    impl->listener = std::move(*listener);
    impl->listening = true;
    _impl = std::move(impl);
    _address.clear();
    return {};
}

bool Socket::is_open() const
{
    return _impl != nullptr && _impl->open;
}

bool Socket::is_listening() const
{
    return this->is_open() && _impl->listening;
}

uint16_t Socket::psm() const
{
    return _impl != nullptr ? _impl->psm : 0;
}

Result<size_t> Socket::send(ArrUByteView data)
{
    auto streaming = this->_check_stream();
    SIHD_UNEXPECTED_RETURN(streaming);
    auto runtime = java::runtime();
    SIHD_UNEXPECTED_RETURN(runtime);
    java::Env env(runtime->vm);
    if (!env)
        return std::unexpected(java::detached());
    jbyteArray bytes = java::byte_array(env.get(), data);
    env->CallVoidMethod(_impl->stream.get(), env->GetMethodID(runtime->classes.stream, "write", "([B)V"), bytes);
    env->DeleteLocalRef(bytes);
    auto sent = java::check(env.get(), "bluetooth send");
    SIHD_UNEXPECTED_RETURN(sent);
    return data.size();
}

Result<size_t> Socket::receive(IArray & array)
{
    auto streaming = this->_check_stream();
    SIHD_UNEXPECTED_RETURN(streaming);
    auto runtime = java::runtime();
    SIHD_UNEXPECTED_RETURN(runtime);
    java::Env env(runtime->vm);
    if (!env)
        return std::unexpected(java::detached());

    jbyteArray buffer = env->NewByteArray(static_cast<jsize>(array.byte_capacity()));
    const jint received = env->CallIntMethod(_impl->stream.get(),
                                             env->GetMethodID(runtime->classes.stream, "read", "([B)I"),
                                             buffer);
    auto read = java::check(env.get(), "bluetooth receive");
    if (read)
    {
        env->GetByteArrayRegion(buffer, 0, received, reinterpret_cast<jbyte *>(array.buf()));
        array.byte_resize(static_cast<size_t>(received));
    }
    env->DeleteLocalRef(buffer);

    if (!read)
    {
        auto still_streaming = this->_check_stream();
        SIHD_UNEXPECTED_RETURN(still_streaming);
    }
    SIHD_UNEXPECTED_RETURN(read);
    return static_cast<size_t>(received);
}

Result<Socket> Socket::accept(Duration timeout)
{
    auto listening = this->_check_listening();
    SIHD_UNEXPECTED_RETURN(listening);
    auto runtime = java::runtime();
    SIHD_UNEXPECTED_RETURN(runtime);
    java::Env env(runtime->vm);
    if (!env)
        return std::unexpected(java::detached());

    jmethodID accept = env->GetMethodID(runtime->classes.listener, "accept", "(I)Lsihd/bt/Stream;");
    jobject local = env->CallObjectMethod(_impl->listener.get(), accept, static_cast<jint>(timeout.milliseconds_int()));
    auto accepted = java::check(env.get(), "bluetooth accept");
    auto still_listening = this->_check_listening();
    if (!still_listening && local != nullptr)
        env->DeleteLocalRef(local);
    SIHD_UNEXPECTED_RETURN(still_listening);
    SIHD_UNEXPECTED_RETURN(accepted);

    if (local == nullptr)
    {
        if (_impl->psm != 0)
            return std::unexpected(Error(ErrorCode::timeout, "no L2CAP connection on psm {}", _impl->psm));
        return std::unexpected(Error(ErrorCode::timeout, "no RFCOMM connection"));
    }
    auto address = static_cast<jstring>(
        env->CallObjectMethod(local, env->GetMethodID(runtime->classes.stream, "address", "()Ljava/lang/String;")));
    std::string peer = java::string(env.get(), address);
    env->DeleteLocalRef(address);
    auto impl = std::make_unique<Impl>();
    impl->stream = java::Object(env.get(), local);
    impl->psm = _impl->psm;
    Socket client;
    client._impl = std::move(impl);
    client._address = str::to_upper_copy(peer);
    return client;
}

void Socket::close()
{
    if (_impl == nullptr || !_impl->open.exchange(false))
        return;
    auto runtime = java::runtime();
    if (!runtime)
        return;
    java::Env env(runtime->vm);
    if (!env)
        return;
    if (_impl->listening)
    {
        env->CallVoidMethod(_impl->listener.get(), env->GetMethodID(runtime->classes.listener, "close", "()V"));
        (void)java::check(env.get(), "bluetooth listener close");
        return;
    }
    env->CallVoidMethod(_impl->stream.get(), env->GetMethodID(runtime->classes.stream, "close", "()V"));
    (void)java::check(env.get(), "bluetooth close");
}

} // namespace sihd::bt
