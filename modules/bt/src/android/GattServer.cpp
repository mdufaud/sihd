#include <algorithm>
#include <mutex>

#include <sihd/bt/GattServer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

#include "../att.hpp"
#include "java.hpp"
#include "natives.hpp"

namespace sihd::bt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;

SIHD_LOGGER;

namespace
{

const Duration add_timeout(time::sec(10));

struct Published
{
        std::string service_uuid;
        LocalCharacteristic characteristic;
        std::vector<uint8_t> value;
};

// the java callbacks hold the java server lock: never call java under mutex
struct ServerSink
{
        java::Completion completion;
        std::mutex mutex;
        std::vector<Published> published;

        Published *find(const std::string & service_uuid, const std::string & characteristic_uuid)
        {
            auto it = std::ranges::find_if(published, [&](const Published & candidate) {
                return str::to_lower_copy(candidate.service_uuid) == str::to_lower_copy(service_uuid)
                       && str::to_lower_copy(candidate.characteristic.uuid) == str::to_lower_copy(characteristic_uuid);
            });
            return it != published.end() ? &*it : nullptr;
        }
};

Result<java::Object> open_server(JNIEnv *env, const java::Runtime & runtime, ServerSink & sink)
{
    jclass local_gatt = runtime.classes.local_gatt;
    java::Object server(
        env,
        env->NewObject(local_gatt, env->GetMethodID(local_gatt, "<init>", "(J)V"), reinterpret_cast<jlong>(&sink)));
    auto created = java::check(env, "gatt server");
    SIHD_UNEXPECTED_RETURN(created);
    env->CallVoidMethod(server.get(),
                        env->GetMethodID(local_gatt, "open", "(Landroid/content/Context;)V"),
                        runtime.context);
    auto opened = java::check(env, "gatt server");
    SIHD_UNEXPECTED_RETURN(opened);
    return server;
}

// the system adds one service at a time, each answered by local_gatt_on_complete
Result<void> publish_service(JNIEnv *env,
                             jobject server,
                             jmethodID add,
                             const LocalService & service,
                             java::Completion & completion)
{
    std::vector<std::string> uuids;
    std::vector<jint> properties;
    for (const LocalCharacteristic & characteristic : service.characteristics)
    {
        uuids.emplace_back(characteristic.uuid);
        properties.emplace_back(characteristic.properties);
    }
    jstring uuid = java::jstring_of(env, service.uuid);
    jobjectArray java_uuids = java::string_array(env, uuids);
    jintArray java_properties = env->NewIntArray(static_cast<jsize>(properties.size()));
    env->SetIntArrayRegion(java_properties, 0, static_cast<jsize>(properties.size()), properties.data());

    completion.expect();
    env->CallVoidMethod(server, add, uuid, service.primary ? JNI_TRUE : JNI_FALSE, java_uuids, java_properties);
    env->DeleteLocalRef(uuid);
    env->DeleteLocalRef(java_uuids);
    env->DeleteLocalRef(java_properties);
    auto added = java::check(env, "gatt service");
    SIHD_UNEXPECTED_RETURN(added);

    auto answer = completion.wait(add_timeout, "gatt service");
    SIHD_UNEXPECTED_RETURN(answer);
    if (answer->status != 0)
        return std::unexpected(att::error_of(answer->status, "gatt service"));
    return {};
}

} // namespace

namespace natives
{

void JNICALL local_gatt_on_complete(JNIEnv *, jclass, jlong handle, jint status)
{
    reinterpret_cast<ServerSink *>(handle)->completion.complete(status, {});
}

jbyteArray JNICALL local_gatt_on_read(JNIEnv *env, jclass, jlong handle, jstring service, jstring characteristic)
{
    auto *sink = reinterpret_cast<ServerSink *>(handle);
    std::vector<uint8_t> value;
    std::function<std::vector<uint8_t>()> on_read;
    {
        std::lock_guard lock(sink->mutex);
        Published *found = sink->find(java::string(env, service), java::string(env, characteristic));
        if (found == nullptr)
            return nullptr;
        on_read = found->characteristic.on_read;
        value = found->value;
    }
    if (on_read)
        value = on_read();
    return java::byte_array(env, value);
}

jboolean JNICALL
    local_gatt_on_write(JNIEnv *env, jclass, jlong handle, jstring service, jstring characteristic, jbyteArray value)
{
    auto *sink = reinterpret_cast<ServerSink *>(handle);
    const std::string service_uuid = java::string(env, service);
    const std::string characteristic_uuid = java::string(env, characteristic);
    std::vector<uint8_t> written = java::bytes(env, value);
    std::function<bool(const std::vector<uint8_t> &)> on_write;
    {
        std::lock_guard lock(sink->mutex);
        Published *found = sink->find(service_uuid, characteristic_uuid);
        if (found == nullptr)
            return JNI_FALSE;
        on_write = found->characteristic.on_write;
    }
    if (on_write && !on_write(written))
        return JNI_FALSE;
    std::lock_guard lock(sink->mutex);
    if (Published *found = sink->find(service_uuid, characteristic_uuid))
        found->value = std::move(written);
    return JNI_TRUE;
}

} // namespace natives

struct GattServer::Impl
{
        ServerSink sink;
        Waitable waitable;
        // the started java server: notify() takes a local reference of it, never calls java under this mutex
        std::mutex server_mutex;
        jobject server = nullptr;
};

GattServer::GattServer(const std::string & name, Node *parent): Named(name, parent), _impl(std::make_unique<Impl>())
{
    this->set_adapter(0);
    this->add_conf("adapter", &GattServer::set_adapter);
}

GattServer::~GattServer()
{
    if (this->is_running())
        this->stop();
}

Result<void>
    GattServer::notify(const std::string & service_uuid, const std::string & characteristic_uuid, ArrUByteView value)
{
    auto runtime = java::runtime();
    SIHD_UNEXPECTED_RETURN(runtime);
    java::Env env(runtime->vm);
    if (!env)
        return std::unexpected(java::detached());

    jobject server = nullptr;
    {
        std::lock_guard server_lock(_impl->server_mutex);
        if (_impl->server == nullptr)
            return std::unexpected(Error(closed, "gatt server is not started"));
        std::lock_guard lock(_impl->sink.mutex);
        Published *found = _impl->sink.find(service_uuid, characteristic_uuid);
        if (found == nullptr)
            return std::unexpected(
                Error(not_found, "no characteristic {} in service {}", characteristic_uuid, service_uuid));
        if ((found->characteristic.properties & (GattCharacteristic::notify | GattCharacteristic::indicate)) == 0)
            return std::unexpected(
                Error(invalid_argument, "characteristic {} neither notifies nor indicates", characteristic_uuid));
        found->value.assign(value.begin(), value.end());
        server = env->NewLocalRef(_impl->server);
    }

    jstring java_service = java::jstring_of(env.get(), service_uuid);
    jstring java_characteristic = java::jstring_of(env.get(), characteristic_uuid);
    jbyteArray bytes = java::byte_array(env.get(), value);
    jmethodID notify = env->GetMethodID(runtime->classes.local_gatt,
                                        "notify",
                                        "(Ljava/lang/String;Ljava/lang/String;[B)Z");
    (void)env->CallBooleanMethod(server, notify, java_service, java_characteristic, bytes);
    env->DeleteLocalRef(java_service);
    env->DeleteLocalRef(java_characteristic);
    env->DeleteLocalRef(bytes);
    env->DeleteLocalRef(server);
    return java::check(env.get(), "gatt notification");
}

bool GattServer::on_start()
{
    if (_adapter_index != 0)
    {
        SIHD_LOG(error, "GattServer: no bluetooth adapter at index {}", _adapter_index);
        return false;
    }
    auto runtime = java::runtime();
    if (SIHD_UNEXPECTED_LOG(runtime))
        return false;
    java::Env env(runtime->vm);
    if (!env)
        return false;

    ServerSink & sink = _impl->sink;
    sink.completion.reset();
    {
        std::lock_guard lock(sink.mutex);
        sink.published.clear();
        for (const LocalService & service : _services)
        {
            for (const LocalCharacteristic & characteristic : service.characteristics)
                sink.published.push_back(
                    Published {.service_uuid = service.uuid, .characteristic = characteristic, .value = {}});
        }
    }
    auto server = open_server(env.get(), *runtime, sink);
    if (SIHD_UNEXPECTED_LOG(server))
        return false;

    bool served = true;
    jmethodID add = env->GetMethodID(runtime->classes.local_gatt,
                                     "addService",
                                     "(Ljava/lang/String;Z[Ljava/lang/String;[I)V");
    for (const LocalService & service : _services)
    {
        auto added = publish_service(env.get(), server->get(), add, service, sink.completion);
        served = !SIHD_UNEXPECTED_LOG(added);
        if (!served)
            break;
    }

    if (served)
    {
        {
            std::lock_guard lock(_impl->server_mutex);
            _impl->server = server->get();
        }
        this->service_set_ready();
        _impl->waitable.wait([this] { return this->service_stop_requested(); });
        std::lock_guard lock(_impl->server_mutex);
        _impl->server = nullptr;
    }

    // the java side is quiet once close() returned: the sink stays for the next start
    env->CallVoidMethod(server->get(), env->GetMethodID(runtime->classes.local_gatt, "close", "()V"));
    (void)java::check(env.get(), "gatt server close");
    return served;
}

bool GattServer::on_stop()
{
    {
        // the stop request is visible to the waiting on_start() once it holds the guard
        auto lock = _impl->waitable.guard();
    }
    _impl->waitable.notify_all();
    return true;
}

} // namespace sihd::bt
