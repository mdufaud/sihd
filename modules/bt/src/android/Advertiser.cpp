#include <sihd/bt/Advertiser.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/time.hpp>

#include "java.hpp"
#include "natives.hpp"

namespace sihd::bt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;

SIHD_LOGGER;

namespace
{

const Duration start_timeout(time::sec(10));

jobjectArray byte_arrays(JNIEnv *env, const std::vector<std::vector<uint8_t>> & values)
{
    jclass bytes_class = env->FindClass("[B");
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(values.size()), bytes_class, nullptr);
    env->DeleteLocalRef(bytes_class);
    for (size_t i = 0; i < values.size(); ++i)
    {
        jbyteArray element = java::byte_array(env, values[i]);
        env->SetObjectArrayElement(array, static_cast<jsize>(i), element);
        env->DeleteLocalRef(element);
    }
    return array;
}

// AdvertiseCallback.ADVERTISE_FAILED_*
Error advertising_error(int status)
{
    switch (status)
    {
        case 1:
            return Error(invalid_argument, "bluetooth advertising: the data is larger than a legacy advertisement");
        case 2:
            return Error(io_error, "bluetooth advertising: no advertising instance is left");
        case 3:
            return Error(io_error, "bluetooth advertising: already started");
        case 5:
            return Error(not_supported, "bluetooth advertising: this adapter cannot advertise");
        default:
            return Error(io_error, "bluetooth advertising: internal error {}", status);
    }
}

Result<java::Object> create_advertising(JNIEnv *env, const java::Runtime & runtime, java::Completion & completion)
{
    jclass advertising = runtime.classes.advertising;
    java::Object created(env,
                         env->NewObject(advertising,
                                        env->GetMethodID(advertising, "<init>", "(J)V"),
                                        reinterpret_cast<jlong>(&completion)));
    auto checked = java::check(env, "bluetooth advertising");
    SIHD_UNEXPECTED_RETURN(checked);
    return created;
}

// answered by advertising_on_complete
Result<void> start_advertising(JNIEnv *env,
                               const java::Runtime & runtime,
                               jobject advertising,
                               const Advertisement & advertisement,
                               java::Completion & completion)
{
    std::vector<jint> manufacturer_ids;
    std::vector<std::vector<uint8_t>> manufacturer_data;
    for (const auto & [company, data] : advertisement.manufacturer_data)
    {
        manufacturer_ids.emplace_back(company);
        manufacturer_data.emplace_back(data);
    }

    std::vector<std::string> service_data_uuids;
    std::vector<std::vector<uint8_t>> service_data;
    for (const auto & [uuid, data] : advertisement.service_data)
    {
        service_data_uuids.emplace_back(uuid);
        service_data.emplace_back(data);
    }

    jobjectArray java_uuids = java::string_array(env, advertisement.service_uuids);
    jintArray java_ids = env->NewIntArray(static_cast<jsize>(manufacturer_ids.size()));
    env->SetIntArrayRegion(java_ids, 0, static_cast<jsize>(manufacturer_ids.size()), manufacturer_ids.data());
    jobjectArray java_manufacturer_data = byte_arrays(env, manufacturer_data);
    jobjectArray java_service_data_uuids = java::string_array(env, service_data_uuids);
    jobjectArray java_service_data = byte_arrays(env, service_data);

    completion.reset();
    completion.expect();
    jmethodID start = env->GetMethodID(runtime.classes.advertising,
                                       "start",
                                       "(Landroid/content/Context;Z[Ljava/lang/String;[I[[B[Ljava/lang/String;[[B)V");
    env->CallVoidMethod(advertising,
                        start,
                        runtime.context,
                        advertisement.connectable ? JNI_TRUE : JNI_FALSE,
                        java_uuids,
                        java_ids,
                        java_manufacturer_data,
                        java_service_data_uuids,
                        java_service_data);
    env->DeleteLocalRef(java_uuids);
    env->DeleteLocalRef(java_ids);
    env->DeleteLocalRef(java_manufacturer_data);
    env->DeleteLocalRef(java_service_data_uuids);
    env->DeleteLocalRef(java_service_data);
    auto started = java::check(env, "bluetooth advertising");
    SIHD_UNEXPECTED_RETURN(started);

    auto answer = completion.wait(start_timeout, "bluetooth advertising");
    SIHD_UNEXPECTED_RETURN(answer);
    if (answer->status != 0)
        return std::unexpected(advertising_error(answer->status));
    return {};
}

} // namespace

namespace natives
{

void JNICALL advertising_on_complete(JNIEnv *, jclass, jlong handle, jint status)
{
    reinterpret_cast<java::Completion *>(handle)->complete(status, {});
}

} // namespace natives

struct Advertiser::Impl
{
        java::Completion completion;
        Waitable waitable;
};

Advertiser::Advertiser(const std::string & name, Node *parent): Named(name, parent), _impl(std::make_unique<Impl>())
{
    this->set_adapter(0);
    this->add_conf("adapter", &Advertiser::set_adapter);
}

Advertiser::~Advertiser()
{
    if (this->is_running())
        this->stop();
}

bool Advertiser::on_start()
{
    if (_adapter_index != 0)
    {
        SIHD_LOG(error, "Advertiser: no bluetooth adapter at index {}", _adapter_index);
        return false;
    }
    if (!_advertisement.local_name.empty())
    {
        SIHD_LOG(error, "Advertiser: android advertises no local name");
        return false;
    }
    auto runtime = java::runtime();
    if (SIHD_UNEXPECTED_LOG(runtime))
        return false;
    java::Env env(runtime->vm);
    if (!env)
        return false;

    auto advertising = create_advertising(env.get(), *runtime, _impl->completion);
    if (SIHD_UNEXPECTED_LOG(advertising))
        return false;
    auto started = start_advertising(env.get(), *runtime, advertising->get(), _advertisement, _impl->completion);
    const bool advertised = !SIHD_UNEXPECTED_LOG(started);
    if (advertised)
    {
        this->service_set_ready();
        _impl->waitable.wait([this] { return this->service_stop_requested(); });
    }

    // the java side is quiet once stop() returned: the completion stays for the next start
    env->CallVoidMethod(advertising->get(), env->GetMethodID(runtime->classes.advertising, "stop", "()V"));
    (void)java::check(env.get(), "bluetooth advertising stop");
    return advertised;
}

bool Advertiser::on_stop()
{
    {
        // the stop request is visible to the waiting on_start() once it holds the guard
        auto lock = _impl->waitable.guard();
    }
    _impl->waitable.notify_all();
    return true;
}

} // namespace sihd::bt
