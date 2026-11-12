#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <sstream>

#include <sihd/bt/Device.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
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

const Duration connect_timeout(time::sec(30));
const Duration operation_timeout(time::sec(30));
const Duration pair_timeout(time::sec(60));
const Duration max_wait(time::hours(1));
// the largest ATT mtu: android negotiates none by itself
constexpr jint wanted_mtu = 517;
// BluetoothDevice.BOND_NONE and BOND_BONDED
constexpr int bond_none = 10;
constexpr int bond_bonded = 12;

struct GattLink
{
        java::Completion completion;
        Waitable waitable;
        // guarded by waitable
        bool connected = false;
        bool lost = false;
        bool woken = false;
        int status = 0;

        std::mutex subscriptions_mutex;
        // characteristic handle -> uuid
        std::map<uint16_t, std::string> subscribed;
        std::function<void(GattNotification &&)> notify;
};

struct BondSink
{
        Waitable waitable;
        // guarded by waitable
        std::optional<int> state;
};

Error not_connected()
{
    return Error(closed, "bluetooth device is not connected");
}

Result<void> known(const std::set<uint16_t> & handles, uint16_t handle)
{
    if (!handles.contains(handle))
        return std::unexpected(Error(not_found, "no gatt attribute of handle {}, services() lists them", handle));
    return {};
}

// the lines of Gatt.attributes(): "s uuid handle primary", "c uuid handle properties", "d uuid handle"
std::vector<GattService> parse_attributes(const std::vector<std::string> & lines,
                                          std::set<uint16_t> & characteristics,
                                          std::set<uint16_t> & descriptors)
{
    std::vector<GattService> services;
    for (const std::string & line : lines)
    {
        std::istringstream fields(line);
        char kind = 0;
        std::string uuid;
        unsigned int handle = 0;
        unsigned int extra = 0;
        fields >> kind >> uuid >> handle >> extra;

        if (kind == 's')
            services.push_back(GattService {.uuid = uuid,
                                            .handle = static_cast<uint16_t>(handle),
                                            .primary = extra == 1,
                                            .characteristics = {}});
        else if (kind == 'c' && !services.empty())
        {
            characteristics.insert(static_cast<uint16_t>(handle));
            services.back().characteristics.push_back(GattCharacteristic {
                .uuid = uuid,
                .handle = static_cast<uint16_t>(handle),
                .properties = static_cast<uint8_t>(extra),
                .descriptors = {},
            });
        }
        else if (kind == 'd' && !services.empty() && !services.back().characteristics.empty())
        {
            descriptors.insert(static_cast<uint16_t>(handle));
            services.back().characteristics.back().descriptors.push_back(
                GattDescriptor {.uuid = uuid, .handle = static_cast<uint16_t>(handle)});
        }
    }
    return services;
}

} // namespace

namespace natives
{

void JNICALL gatt_on_connection(JNIEnv *, jclass, jlong handle, jboolean connected, jint status)
{
    auto *link = reinterpret_cast<GattLink *>(handle);
    {
        auto lock = link->waitable.guard();
        link->status = status;
        if (connected == JNI_TRUE)
            link->connected = true;
        else
            link->lost = true;
    }
    link->waitable.notify_all();
    if (connected != JNI_TRUE)
        link->completion.abort();
}

void JNICALL gatt_on_complete(JNIEnv *env, jclass, jlong handle, jint status, jbyteArray value)
{
    reinterpret_cast<GattLink *>(handle)->completion.complete(status, java::bytes(env, value));
}

void JNICALL gatt_on_notification(JNIEnv *env, jclass, jlong handle, jint instance, jbyteArray value)
{
    auto *link = reinterpret_cast<GattLink *>(handle);
    const auto characteristic_handle = static_cast<uint16_t>(instance);
    std::string uuid;
    {
        std::lock_guard lock(link->subscriptions_mutex);
        auto it = link->subscribed.find(characteristic_handle);
        if (it == link->subscribed.end())
            return;
        uuid = it->second;
    }
    link->notify(GattNotification {
        .handle = characteristic_handle,
        .uuid = std::move(uuid),
        .value = java::bytes(env, value),
        .timestamp = {},
    });
}

void JNICALL bond_on_state(JNIEnv *, jclass, jlong handle, jint state)
{
    auto *sink = reinterpret_cast<BondSink *>(handle);
    {
        auto lock = sink->waitable.guard();
        sink->state = state;
    }
    sink->waitable.notify_all();
}

} // namespace natives

struct Device::Impl
{
        GattLink link;
        java::Runtime runtime;

        // android runs one gatt operation at a time per connection
        std::mutex calls_mutex;
        java::Object gatt;
        // from the last services()
        std::set<uint16_t> characteristics;
        std::set<uint16_t> descriptors;

        // calls_mutex held: starts the operation on the gatt object, waits for its nativeOnComplete
        Result<java::Completion::Answer>
            run(JNIEnv *env, std::string_view what, const std::function<void(JNIEnv *, jobject)> & start)
        {
            if (!gatt)
                return std::unexpected(not_connected());
            link.completion.expect();
            start(env, gatt.get());
            auto started = java::check(env, what);
            SIHD_UNEXPECTED_RETURN(started);
            auto answer = link.completion.wait(operation_timeout, what);
            SIHD_UNEXPECTED_RETURN(answer);
            if (answer->status != 0)
                return std::unexpected(att::error_of(answer->status, what));
            return answer;
        }

        jmethodID method(JNIEnv *env, const char *name, const char *signature) const
        {
            return env->GetMethodID(runtime.classes.gatt, name, signature);
        }

        // an operation of a consumer thread on an attribute services() listed
        Result<java::Completion::Answer> operate(std::string_view what,
                                                 const std::set<uint16_t> Impl::*handles,
                                                 uint16_t handle,
                                                 const std::function<void(JNIEnv *, jobject)> & start)
        {
            auto current = java::runtime();
            SIHD_UNEXPECTED_RETURN(current);
            java::Env env(current->vm);
            if (!env)
                return std::unexpected(java::detached());
            std::lock_guard lock(calls_mutex);
            if (!gatt)
                return std::unexpected(not_connected());
            auto listed = known(this->*handles, handle);
            SIHD_UNEXPECTED_RETURN(listed);
            return this->run(env.get(), what, start);
        }
};

Device::Device(const std::string & name, Node *parent): Named(name, parent), _impl(std::make_unique<Impl>())
{
    this->_add_confs();
    _impl->link.notify = [this](GattNotification && notification) {
        this->_queue_notification(std::move(notification));
    };
}

Device::~Device()
{
    if (this->is_running())
        this->stop();
}

Result<DeviceInfo> Device::info() const
{
    auto runtime = java::runtime();
    SIHD_UNEXPECTED_RETURN(runtime);
    java::Env env(runtime->vm);
    if (!env)
        return std::unexpected(java::detached());
    jmethodID device_info = env->GetStaticMethodID(runtime->classes.adapters,
                                                   "deviceInfo",
                                                   "(Landroid/content/Context;Ljava/lang/String;)[Ljava/lang/String;");
    jstring address = java::jstring_of(env.get(), _address);
    auto fields = static_cast<jobjectArray>(
        env->CallStaticObjectMethod(runtime->classes.adapters, device_info, runtime->context, address));
    env->DeleteLocalRef(address);
    auto read = java::check(env.get(), "bluetooth device");
    SIHD_UNEXPECTED_RETURN(read);
    const std::vector<std::string> values = java::strings(env.get(), fields);
    env->DeleteLocalRef(fields);

    // name, address type, bonded, connected, class of device, then the uuids
    DeviceInfo info;
    info.address = _address;
    info.name = values.at(0);
    const int address_type = std::stoi(values.at(1));
    info.address_type = address_type == 0   ? AddressType::public_address
                        : address_type == 1 ? AddressType::random
                                            : AddressType::unspecified;
    info.paired = values.at(2) == "1";
    info.connected = values.at(3) == "1";
    if (const int class_of_device = std::stoi(values.at(4)); class_of_device >= 0)
        info.class_of_device = static_cast<uint32_t>(class_of_device);
    info.uuids.assign(values.begin() + 5, values.end());
    return info;
}

Result<void> Device::pair()
{
    if (_pairing_handler)
        return std::unexpected(Error(not_supported, "android pairs through its system dialog: no pairing handler"));
    auto runtime = java::runtime();
    SIHD_UNEXPECTED_RETURN(runtime);
    java::Env env(runtime->vm);
    if (!env)
        return std::unexpected(java::detached());

    BondSink sink;
    jmethodID constructor = env->GetMethodID(runtime->classes.bond,
                                             "<init>",
                                             "(Landroid/content/Context;Ljava/lang/String;J)V");
    jstring address = java::jstring_of(env.get(), _address);
    java::Object bond(
        env.get(),
        env->NewObject(runtime->classes.bond, constructor, runtime->context, address, reinterpret_cast<jlong>(&sink)));
    env->DeleteLocalRef(address);
    auto created = java::check(env.get(), "pairing");
    SIHD_UNEXPECTED_RETURN(created);

    const jboolean bonding = env->CallBooleanMethod(bond.get(),
                                                    env->GetMethodID(runtime->classes.bond, "start", "()Z"));
    auto started = java::check(env.get(), "pairing");
    std::optional<int> state;
    if (started && bonding == JNI_TRUE)
    {
        auto lock = sink.waitable.wait_for_guard(pair_timeout, [&sink] {
            return sink.state == bond_bonded || sink.state == bond_none;
        });
        state = sink.state;
    }

    // quiet once stop() returned: the sink can go
    env->CallVoidMethod(bond.get(), env->GetMethodID(runtime->classes.bond, "stop", "()V"));
    (void)java::check(env.get(), "pairing");
    SIHD_UNEXPECTED_RETURN(started);
    if (bonding != JNI_TRUE)
        return std::unexpected(Error(already_exists, "{} is already paired", _address));
    if (state == bond_bonded)
        return {};
    if (state == bond_none)
        return std::unexpected(Error(permission_denied, "the pairing of {} failed or was refused", _address));
    return std::unexpected(Error(timeout, "the pairing of {} did not end", _address));
}

Result<void> Device::cancel_pairing()
{
    return std::unexpected(Error(not_supported, "android cancels a pairing from its system dialog only"));
}

Result<void> Device::unpair()
{
    return std::unexpected(Error(not_supported, "android removes a pairing from its settings only"));
}

Result<void> Device::set_trusted(bool)
{
    return std::unexpected(Error(not_supported, "android has no trusted devices"));
}

Result<std::vector<GattService>> Device::services()
{
    auto runtime = java::runtime();
    SIHD_UNEXPECTED_RETURN(runtime);
    java::Env env(runtime->vm);
    if (!env)
        return std::unexpected(java::detached());

    std::lock_guard lock(_impl->calls_mutex);
    if (!_impl->gatt)
        return std::unexpected(not_connected());
    auto lines = static_cast<jobjectArray>(
        env->CallObjectMethod(_impl->gatt.get(), _impl->method(env.get(), "attributes", "()[Ljava/lang/String;")));
    auto listed = java::check(env.get(), "gatt services");
    SIHD_UNEXPECTED_RETURN(listed);

    const std::vector<std::string> attributes = java::strings(env.get(), lines);
    env->DeleteLocalRef(lines);
    _impl->characteristics.clear();
    _impl->descriptors.clear();
    return parse_attributes(attributes, _impl->characteristics, _impl->descriptors);
}

Result<std::vector<uint8_t>> Device::read_characteristic(const GattCharacteristic & characteristic)
{
    auto answer = _impl->operate(
        "read",
        &Impl::characteristics,
        characteristic.handle,
        [this, &characteristic](JNIEnv *env, jobject gatt) {
            env->CallVoidMethod(gatt, _impl->method(env, "read", "(I)V"), static_cast<jint>(characteristic.handle));
        });
    SIHD_UNEXPECTED_RETURN(answer);
    return std::move(answer->value);
}

Result<void> Device::write_characteristic(const GattCharacteristic & characteristic, ArrUByteView value, WriteType type)
{
    auto answer = _impl->operate("write",
                                 &Impl::characteristics,
                                 characteristic.handle,
                                 [&](JNIEnv *env, jobject gatt) {
                                     jbyteArray bytes = java::byte_array(env, value);
                                     env->CallVoidMethod(gatt,
                                                         _impl->method(env, "write", "(I[BZ)V"),
                                                         static_cast<jint>(characteristic.handle),
                                                         bytes,
                                                         type == WriteType::with_response ? JNI_TRUE : JNI_FALSE);
                                     env->DeleteLocalRef(bytes);
                                 });
    SIHD_UNEXPECTED_RETURN(answer);
    return {};
}

Result<std::vector<uint8_t>> Device::read_descriptor(const GattDescriptor & descriptor)
{
    auto answer = _impl->operate("descriptor read",
                                 &Impl::descriptors,
                                 descriptor.handle,
                                 [this, &descriptor](JNIEnv *env, jobject gatt) {
                                     env->CallVoidMethod(gatt,
                                                         _impl->method(env, "readDescriptor", "(I)V"),
                                                         static_cast<jint>(descriptor.handle));
                                 });
    SIHD_UNEXPECTED_RETURN(answer);
    return std::move(answer->value);
}

Result<void> Device::write_descriptor(const GattDescriptor & descriptor, ArrUByteView value)
{
    auto answer = _impl->operate("descriptor write",
                                 &Impl::descriptors,
                                 descriptor.handle,
                                 [&](JNIEnv *env, jobject gatt) {
                                     jbyteArray bytes = java::byte_array(env, value);
                                     env->CallVoidMethod(gatt,
                                                         _impl->method(env, "writeDescriptor", "(I[B)V"),
                                                         static_cast<jint>(descriptor.handle),
                                                         bytes);
                                     env->DeleteLocalRef(bytes);
                                 });
    SIHD_UNEXPECTED_RETURN(answer);
    return {};
}

Result<void> Device::subscribe(const GattCharacteristic & characteristic)
{
    // a notification may come before the CCCD write answers: listed first
    {
        std::lock_guard lock(_impl->link.subscriptions_mutex);
        _impl->link.subscribed.insert_or_assign(characteristic.handle, characteristic.uuid);
    }
    auto answer = _impl->operate("subscription",
                                 &Impl::characteristics,
                                 characteristic.handle,
                                 [this, &characteristic](JNIEnv *env, jobject gatt) {
                                     env->CallVoidMethod(gatt,
                                                         _impl->method(env, "subscribe", "(IZ)V"),
                                                         static_cast<jint>(characteristic.handle),
                                                         JNI_TRUE);
                                 });
    if (!answer)
    {
        std::lock_guard lock(_impl->link.subscriptions_mutex);
        _impl->link.subscribed.erase(characteristic.handle);
    }
    SIHD_UNEXPECTED_RETURN(answer);
    return {};
}

Result<void> Device::unsubscribe(const GattCharacteristic & characteristic)
{
    {
        std::lock_guard lock(_impl->link.subscriptions_mutex);
        _impl->link.subscribed.erase(characteristic.handle);
    }
    auto answer = _impl->operate("unsubscription",
                                 &Impl::characteristics,
                                 characteristic.handle,
                                 [this, &characteristic](JNIEnv *env, jobject gatt) {
                                     env->CallVoidMethod(gatt,
                                                         _impl->method(env, "subscribe", "(IZ)V"),
                                                         static_cast<jint>(characteristic.handle),
                                                         JNI_FALSE);
                                 });
    SIHD_UNEXPECTED_RETURN(answer);
    return {};
}

Result<uint16_t> Device::mtu()
{
    auto runtime = java::runtime();
    SIHD_UNEXPECTED_RETURN(runtime);
    java::Env env(runtime->vm);
    if (!env)
        return std::unexpected(java::detached());
    std::lock_guard lock(_impl->calls_mutex);
    if (!_impl->gatt)
        return std::unexpected(not_connected());
    const jint mtu = env->CallIntMethod(_impl->gatt.get(), _impl->method(env.get(), "mtu", "()I"));
    auto read = java::check(env.get(), "mtu");
    SIHD_UNEXPECTED_RETURN(read);
    return static_cast<uint16_t>(mtu);
}

bool Device::on_start()
{
    auto runtime = java::runtime();
    if (SIHD_UNEXPECTED_LOG(runtime))
        return false;
    java::Env env(runtime->vm);
    if (!env)
        return false;
    GattLink & link = _impl->link;
    _impl->runtime = *runtime;
    link.completion.reset();
    {
        auto lock = link.waitable.guard();
        link.connected = false;
        link.lost = false;
        link.woken = false;
    }

    jmethodID constructor = env->GetMethodID(runtime->classes.gatt, "<init>", "(J)V");
    java::Object gatt(env.get(), env->NewObject(runtime->classes.gatt, constructor, reinterpret_cast<jlong>(&link)));
    auto created = java::check(env.get(), "gatt connection");
    if (SIHD_UNEXPECTED_LOG(created))
        return false;
    jstring address = java::jstring_of(env.get(), _address);
    env->CallVoidMethod(gatt.get(),
                        _impl->method(env.get(), "connect", "(Landroid/content/Context;Ljava/lang/String;)V"),
                        runtime->context,
                        address);
    env->DeleteLocalRef(address);
    auto connecting = java::check(env.get(), "gatt connection");

    bool connected = false;
    if (!SIHD_UNEXPECTED_LOG(connecting))
    {
        auto lock = link.waitable.wait_for_guard(connect_timeout, [this, &link] {
            return link.connected || link.lost || this->service_stop_requested();
        });
        connected = link.connected && !link.lost;
        if (!connected && !this->service_stop_requested())
            SIHD_LOG(error, "Device: cannot connect {}: gatt status 0x{:02x}", _address, link.status);
    }

    if (connected)
    {
        std::lock_guard lock(_impl->calls_mutex);
        _impl->gatt = std::move(gatt);
        auto mtu = _impl->run(env.get(), "mtu request", [this](JNIEnv *java_env, jobject object) {
            java_env->CallVoidMethod(object, _impl->method(java_env, "requestMtu", "(I)V"), wanted_mtu);
        });
        SIHD_UNEXPECTED_LOG(mtu);

        auto discovered = _impl->run(env.get(), "service discovery", [this](JNIEnv *java_env, jobject object) {
            java_env->CallVoidMethod(object, _impl->method(java_env, "discover", "()V"));
        });
        connected = !SIHD_UNEXPECTED_LOG(discovered);
        if (!connected)
            gatt = std::move(_impl->gatt);
    }

    if (connected)
    {
        this->_publish_event(Event::connected);
        this->service_set_ready();
    }
    while (connected && !this->service_stop_requested())
    {
        this->_flush_notifications();
        auto lock = link.waitable.wait_for_guard(max_wait, [this, &link] {
            return link.lost || link.woken || this->service_stop_requested();
        });
        link.woken = false;
        if (link.lost)
            break;
    }

    // ends the operations of other threads, then the connection
    link.completion.abort();
    {
        std::lock_guard lock(_impl->calls_mutex);
        if (_impl->gatt)
            gatt = std::move(_impl->gatt);
        _impl->characteristics.clear();
        _impl->descriptors.clear();
    }
    if (gatt)
    {
        env->CallVoidMethod(gatt.get(), _impl->method(env.get(), "close", "()V"));
        (void)java::check(env.get(), "gatt close");
    }

    {
        std::lock_guard lock(link.subscriptions_mutex);
        link.subscribed.clear();
    }
    {
        std::lock_guard lock(_pending_mutex);
        _pending.clear();
    }

    const bool stopped = this->service_stop_requested();
    if (connected)
        this->_publish_event(Event::disconnected);
    return connected && stopped;
}

void Device::_wake_loop()
{
    {
        auto lock = _impl->link.waitable.guard();
        _impl->link.woken = true;
    }
    _impl->link.waitable.notify_all();
}

bool Device::on_stop()
{
    this->_wake_loop();
    return true;
}

} // namespace sihd::bt
