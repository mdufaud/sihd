#include <optional>
#include <span>
#include <vector>

#include <fmt/format.h>

#include <sihd/bt/Scanner.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "java.hpp"
#include "natives.hpp"

namespace sihd::bt
{

using namespace sihd::util;

SIHD_LOGGER;

namespace
{

// java Scan.TRANSPORT_*
constexpr jint java_transport_any = 0;
constexpr jint java_transport_le = 1;
constexpr jint java_transport_classic = 2;

struct ScanSink
{
        explicit ScanSink(Waitable & waitable): waitable(waitable) {}

        Waitable & waitable;
        // guarded by waitable
        std::vector<ScanResult> pending;
        std::optional<int> failure;
};

// little endian 16, 32 or 128 bits uuid in the bluez text form
std::string uuid_of(std::span<const uint8_t> bytes)
{
    std::string hex;
    for (size_t i = bytes.size(); i > 0; --i)
        hex += fmt::format("{:02x}", bytes[i - 1]);
    if (bytes.size() < 16)
        return fmt::format("{:0>8}-0000-1000-8000-00805f9b34fb", hex);
    return fmt::format("{}-{}-{}-{}-{}",
                       hex.substr(0, 8),
                       hex.substr(8, 4),
                       hex.substr(12, 4),
                       hex.substr(16, 4),
                       hex.substr(20));
}

void add_uuids(std::span<const uint8_t> data, size_t uuid_size, ScanResult & result)
{
    for (size_t offset = 0; offset + uuid_size <= data.size(); offset += uuid_size)
        result.uuids.emplace_back(uuid_of(data.subspan(offset, uuid_size)));
}

void add_service_data(std::span<const uint8_t> data, size_t uuid_size, ScanResult & result)
{
    if (data.size() < uuid_size)
        return;
    const std::span<const uint8_t> value = data.subspan(uuid_size);
    result.service_data.insert_or_assign(uuid_of(data.first(uuid_size)),
                                         std::vector<uint8_t>(value.begin(), value.end()));
}

// one advertising data structure of the Bluetooth core specification, part C section 11
void read_structure(uint8_t type, std::span<const uint8_t> data, ScanResult & result)
{
    switch (type)
    {
        // incomplete and complete lists of 16, 32 and 128 bits service uuids
        case 0x02:
        case 0x03:
            return add_uuids(data, 2, result);

        case 0x04:
        case 0x05:
            return add_uuids(data, 4, result);

        case 0x06:
        case 0x07:
            return add_uuids(data, 16, result);

        // shortened and complete local name
        case 0x08:
        case 0x09:
            result.identifier.assign(data.begin(), data.end());
            return;

        // tx power level
        case 0x0a:
            if (!data.empty())
                result.tx_power = static_cast<int8_t>(data[0]);
            return;

        // service data of a 16, 32 and 128 bits uuid
        case 0x16:
            return add_service_data(data, 2, result);
        case 0x20:
            return add_service_data(data, 4, result);
        case 0x21:
            return add_service_data(data, 16, result);

        // appearance
        case 0x19:
            if (data.size() >= 2)
                result.appearance = static_cast<uint16_t>(data[0] | (data[1] << 8));
            return;

        // manufacturer specific data: a little endian company identifier then its bytes
        case 0xff:
            if (data.size() >= 2)
            {
                const uint16_t company = static_cast<uint16_t>(data[0] | (data[1] << 8));
                result.manufacturer_data.insert_or_assign(company, std::vector<uint8_t>(data.begin() + 2, data.end()));
            }
            return;

        default:
            return;
    }
}

// length, type then length - 1 bytes, up to a zero length
void read_record(const std::vector<uint8_t> & record, ScanResult & result)
{
    size_t offset = 0;
    while (offset < record.size())
    {
        const size_t length = record[offset];
        if (length == 0 || offset + 1 + length > record.size())
            return;
        read_structure(record[offset + 1], std::span<const uint8_t>(record).subspan(offset + 2, length - 1), result);
        offset += 1 + length;
    }
}

AddressType address_type_of(jint java_type)
{
    // BluetoothDevice.ADDRESS_TYPE_PUBLIC and ADDRESS_TYPE_RANDOM, -1 before android 15
    if (java_type == 0)
        return AddressType::public_address;
    if (java_type == 1)
        return AddressType::random;
    return AddressType::unspecified;
}

jint java_transport_of(Transport transport)
{
    switch (transport)
    {
        case Transport::le:
            return java_transport_le;
        case Transport::classic:
            return java_transport_classic;
        default:
            return java_transport_any;
    }
}

} // namespace

namespace natives
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
                            jbyteArray record)
{
    auto *sink = reinterpret_cast<ScanSink *>(handle);
    ScanResult result;
    result.address = java::string(env, address);
    result.address_type = address_type_of(address_type);
    result.rssi = static_cast<int16_t>(rssi);
    result.connectable = connectable == JNI_TRUE;
    if (class_of_device >= 0)
        result.class_of_device = static_cast<uint32_t>(class_of_device);
    read_record(java::bytes(env, record), result);
    if (const std::string classic_name = java::string(env, name); !classic_name.empty())
        result.identifier = classic_name;
    {
        auto lock = sink->waitable.guard();
        sink->pending.emplace_back(std::move(result));
    }
    sink->waitable.notify_all();
}

void JNICALL scan_on_failure(JNIEnv *, jclass, jlong handle, jint error_code)
{
    auto *sink = reinterpret_cast<ScanSink *>(handle);
    {
        auto lock = sink->waitable.guard();
        sink->failure = error_code;
    }
    sink->waitable.notify_all();
}

} // namespace natives

bool Scanner::_scan()
{
    if (_adapter_index != 0)
    {
        SIHD_LOG(error, "Scanner: no bluetooth adapter at index {}", _adapter_index);
        return false;
    }
    auto runtime = java::runtime();
    if (SIHD_UNEXPECTED_LOG(runtime))
        return false;
    java::Env env(runtime->vm);
    if (!env)
        return false;

    ScanSink sink(_waitable);
    jmethodID constructor = env->GetMethodID(runtime->classes.scan, "<init>", "(Landroid/content/Context;J)V");
    java::Object scan(
        env.get(),
        env->NewObject(runtime->classes.scan, constructor, runtime->context, reinterpret_cast<jlong>(&sink)));
    auto created = java::check(env.get(), "bluetooth scan");
    if (SIHD_UNEXPECTED_LOG(created))
        return false;

    jmethodID start = env->GetMethodID(runtime->classes.scan, "start", "(I[Ljava/lang/String;)V");
    jmethodID stop = env->GetMethodID(runtime->classes.scan, "stop", "()V");
    jobjectArray uuids = java::string_array(env.get(), _filter.uuids);
    env->CallVoidMethod(scan.get(), start, java_transport_of(_filter.transport), uuids);
    env->DeleteLocalRef(uuids);
    auto started = java::check(env.get(), "bluetooth scan");
    if (SIHD_UNEXPECTED_LOG(started))
    {
        env->CallVoidMethod(scan.get(), stop);
        (void)java::check(env.get(), "bluetooth scan stop");
        return false;
    }
    this->service_set_ready();

    bool scanned = true;
    while (true)
    {
        this->_forget_lost();
        auto wait = this->_next_wait(Duration(time::hours(1)));
        if (!wait.has_value())
            break;

        std::vector<ScanResult> received;
        {
            auto lock = _waitable.wait_for_guard(*wait, [this, &sink] {
                return this->service_stop_requested() || !sink.pending.empty() || sink.failure.has_value();
            });
            received.swap(sink.pending);
            if (sink.failure.has_value())
            {
                SIHD_LOG(error, "Scanner: the bluetooth scan failed with the code {}", *sink.failure);
                scanned = false;
            }
        }
        for (ScanResult & result : received)
        {
            if (!_filter.min_rssi.has_value() || result.rssi >= *_filter.min_rssi)
                this->_publish(std::move(result));
        }
        if (!scanned || this->service_stop_requested())
            break;
    }

    // the java side is quiet once stop() returned: the sink can go
    env->CallVoidMethod(scan.get(), stop);
    (void)java::check(env.get(), "bluetooth scan stop");
    return scanned;
}

} // namespace sihd::bt
