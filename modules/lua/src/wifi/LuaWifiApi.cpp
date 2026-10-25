#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

// clang-format off
#include <sihd/lua/wifi/LuaWifiApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/lua/checked.hpp>
#include <luabridge3/LuaBridge/Vector.h>
// clang-format on

#include <optional>

#include <sihd/lua/LuaGil.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/wifi/Monitor.hpp>
#include <sihd/wifi/capabilities.hpp>
#include <sihd/wifi/connection.hpp>
#include <sihd/wifi/data.hpp>
#include <sihd/wifi/dissect.hpp>
#include <sihd/wifi/ies.hpp>
#include <sihd/wifi/link.hpp>
#include <sihd/wifi/management.hpp>
#include <sihd/wifi/radiotap.hpp>
#include <sihd/wifi/scan.hpp>
#include <sihd/wifi/taxonomy.hpp>

namespace sihd::lua
{

using namespace sihd::util;

namespace
{

using Buffer = std::shared_ptr<const std::string>;

// the decoders return views of their input: the object keeps the bytes they look at
template <typename T>
struct Owned: T
{
        Buffer buffer;
};

Buffer buffer_of(const std::string & data)
{
    return std::make_shared<const std::string>(data);
}

Result<Duration> duration_of(const luabridge::LuaRef & timeout)
{
    if (timeout.isNumber())
        return Duration(timeout.cast<int64_t>().value());
    if (!timeout.isInstance<Duration>())
        return std::unexpected(
            Error(ErrorCode::invalid_argument, "a timeout is a number of nanoseconds or a Duration"));
    return timeout.cast<Duration>().value();
}

// luabridge copies the values it returns: the copies must share one monitor
class LuaWifiMonitor
{
    public:
        static Result<LuaWifiMonitor>
            open(lua_State *state, const std::string & interface_name, luabridge::LuaRef run_timeout)
        {
            LuaWifiMonitor monitor;
            monitor._monitor = std::make_shared<wifi::Monitor>();
            if (!run_timeout.isNil())
            {
                Result<Duration> timeout = duration_of(run_timeout);
                SIHD_UNEXPECTED_RETURN(timeout);
                monitor._monitor->set_run_timeout(*timeout);
            }
            LuaGilRelease release(state);
            auto opened = monitor._monitor->open(interface_name);
            SIHD_UNEXPECTED_RETURN(opened);
            return monitor;
        }

        static Result<void> set_run_timeout(LuaWifiMonitor monitor, luabridge::LuaRef timeout)
        {
            Result<Duration> duration = duration_of(timeout);
            SIHD_UNEXPECTED_RETURN(duration);
            monitor._monitor->set_run_timeout(*duration);
            return {};
        }

        bool run(lua_State *state)
        {
            LuaGilRelease release(state);
            return _monitor->run();
        }
        std::vector<wifi::Monitor::Event> events() const { return _monitor->events(); }

    private:
        std::shared_ptr<wifi::Monitor> _monitor;
};

Result<void> connect(lua_State *state,
                     const std::string & interface_name,
                     const std::string & ssid,
                     luabridge::LuaRef passphrase_or_timeout,
                     luabridge::LuaRef timeout_after_passphrase)
{
    if (timeout_after_passphrase.isNil())
    {
        const Result<Duration> timeout = duration_of(passphrase_or_timeout);
        SIHD_UNEXPECTED_RETURN(timeout);
        LuaGilRelease release(state);
        return wifi::connect(interface_name, ssid, *timeout);
    }
    if (!passphrase_or_timeout.isString())
        return std::unexpected(Error(ErrorCode::invalid_argument, "connect passphrase must be a string"));
    const std::string passphrase = passphrase_or_timeout.tostring();
    const Result<Duration> timeout = duration_of(timeout_after_passphrase);
    SIHD_UNEXPECTED_RETURN(timeout);
    LuaGilRelease release(state);
    return wifi::connect(interface_name, ssid, passphrase, *timeout);
}

template <typename T>
Owned<T> owned(T value, const Buffer & buffer)
{
    Owned<T> result;
    static_cast<T &>(result) = std::move(value);
    result.buffer = buffer;
    return result;
}

template <typename T>
std::vector<Owned<T>> owned_all(std::vector<T> values, const Buffer & buffer)
{
    std::vector<Owned<T>> result;
    for (T & value : values)
        result.push_back(owned(std::move(value), buffer));
    return result;
}

template <typename T>
Result<Owned<T>> parse(const std::string & data, Result<T> (*parser)(ArrUByteView))
{
    const Buffer buffer = buffer_of(data);
    Result<T> parsed = parser(ArrUByteView(*buffer));
    SIHD_UNEXPECTED_RETURN(parsed);
    return owned(std::move(*parsed), buffer);
}

std::string bytes_of(ArrUByteView view)
{
    return std::string(view.begin(), view.end());
}

std::string bytes_of(const wifi::Oui & oui)
{
    return std::string(oui.begin(), oui.end());
}

std::string mac_str(const wifi::MacAddress & mac)
{
    return net::ip::mac_str(mac);
}

std::optional<std::string> mac_str(const std::optional<wifi::MacAddress> & mac)
{
    if (mac.has_value() == false)
        return std::nullopt;
    return net::ip::mac_str(*mac);
}

template <typename T, typename Decoder>
Result<T> decode(const std::string & data, Decoder decoder)
{
    Result<std::vector<wifi::ies::Element>> elements = wifi::ies::elements(ArrUByteView(data));
    SIHD_UNEXPECTED_RETURN(elements);
    return decoder(*elements);
}

Result<bool> mac_flag(const std::string & mac, bool (*test)(const wifi::MacAddress &))
{
    Result<wifi::MacAddress> parsed = wifi::parse_mac(mac);
    SIHD_UNEXPECTED_RETURN(parsed);
    return test(*parsed);
}

template <typename T, typename Finder>
Result<std::optional<Owned<T>>> find_owned(const std::string & data, Finder finder)
{
    const Buffer buffer = buffer_of(data);
    Result<std::vector<wifi::ies::Element>> elements = wifi::ies::elements(ArrUByteView(*buffer));
    SIHD_UNEXPECTED_RETURN(elements);
    std::optional<T> found = finder(*elements);
    if (found.has_value() == false)
        return std::nullopt;
    return owned(std::move(*found), buffer);
}

luabridge::Namespace wifi_namespace(lua_State *state)
{
    return luabridge::getGlobalNamespace(state).beginNamespace("sihd").beginNamespace("wifi");
}

void bind_radiotap(lua_State *state)
{
    wifi_namespace(state)
        .beginClass<Owned<wifi::Radiotap>>("Radiotap")
        .addConstructor<void (*)()>()
        .addProperty("tsft_us", &wifi::Radiotap::tsft_us, false)
        .addProperty("rate_kbps", &wifi::Radiotap::rate_kbps, false)
        .addProperty("mcs", &wifi::Radiotap::mcs, false)
        .addProperty("frequency_mhz", &wifi::Radiotap::frequency_mhz, false)
        .addProperty("channel_flags", &wifi::Radiotap::channel_flags, false)
        .addProperty("signal_dbm", &wifi::Radiotap::signal_dbm, false)
        .addProperty("noise_dbm", &wifi::Radiotap::noise_dbm, false)
        .addProperty("antenna", &wifi::Radiotap::antenna, false)
        .addProperty("antenna_signals", &wifi::Radiotap::antenna_signals, false)
        .addProperty("short_preamble", &wifi::Radiotap::short_preamble, false)
        .addProperty("wep", &wifi::Radiotap::wep, false)
        .addProperty("fragmented", &wifi::Radiotap::fragmented, false)
        .addProperty("fcs_at_end", &wifi::Radiotap::fcs_at_end, false)
        .addProperty("data_padding", &wifi::Radiotap::data_padding, false)
        .addProperty("bad_fcs", &wifi::Radiotap::bad_fcs, false)
        .addProperty("header_size", &wifi::Radiotap::header_size, false)
        .addProperty(
            "payload",
            +[](const Owned<wifi::Radiotap> *self) { return bytes_of(self->payload); })
        .endClass()
        .beginNamespace("Radiotap")
        .beginClass<wifi::Radiotap::AntennaSignal>("AntennaSignal")
        .addConstructor<void (*)()>()
        .addProperty("antenna", &wifi::Radiotap::AntennaSignal::antenna, false)
        .addProperty("signal_dbm", &wifi::Radiotap::AntennaSignal::signal_dbm, false)
        .endClass()
        .beginClass<wifi::Radiotap::Mcs>("Mcs")
        .addConstructor<void (*)()>()
        .addProperty("index", &wifi::Radiotap::Mcs::index, false)
        .addProperty("bandwidth_mhz", &wifi::Radiotap::Mcs::bandwidth_mhz, false)
        .addProperty("short_gi", &wifi::Radiotap::Mcs::short_gi, false)
        .addProperty("greenfield", &wifi::Radiotap::Mcs::greenfield, false)
        .addProperty("ldpc", &wifi::Radiotap::Mcs::ldpc, false)
        .addProperty("stbc_streams", &wifi::Radiotap::Mcs::stbc_streams, false)
        .addProperty("rate_kbps", &wifi::Radiotap::Mcs::rate_kbps, false)
        .endClass()
        .endNamespace()
        .addFunction("parse_radiotap", checked<+[](const std::string & data) {
                         return parse(data, &wifi::parse_radiotap);
                     }>);
}

void bind_frame(lua_State *state)
{
    wifi_namespace(state)
        .beginClass<Owned<wifi::Frame>>("Frame")
        .addConstructor<void (*)()>()
        .addProperty("type", &wifi::Frame::type, false)
        .addProperty("subtype", &wifi::Frame::subtype, false)
        .addProperty("to_ds", &wifi::Frame::to_ds, false)
        .addProperty("from_ds", &wifi::Frame::from_ds, false)
        .addProperty("more_fragments", &wifi::Frame::more_fragments, false)
        .addProperty("retry", &wifi::Frame::retry, false)
        .addProperty("power_management", &wifi::Frame::power_management, false)
        .addProperty("more_data", &wifi::Frame::more_data, false)
        .addProperty("protected_frame", &wifi::Frame::protected_frame, false)
        .addProperty("order", &wifi::Frame::order, false)
        .addProperty("duration", &wifi::Frame::duration, false)
        .addProperty("sequence_number", &wifi::Frame::sequence_number, false)
        .addProperty("fragment_number", &wifi::Frame::fragment_number, false)
        .addProperty("qos_tid", &wifi::Frame::qos_tid, false)
        .addProperty("amsdu", &wifi::Frame::amsdu, false)
        .addProperty("ht_control", &wifi::Frame::ht_control, false)
        .addProperty("header_size", &wifi::Frame::header_size, false)
        .addProperty(
            "addr1",
            +[](const Owned<wifi::Frame> *self) { return mac_str(self->addr1); })
        .addProperty(
            "addr2",
            +[](const Owned<wifi::Frame> *self) { return mac_str(self->addr2); })
        .addProperty(
            "addr3",
            +[](const Owned<wifi::Frame> *self) { return mac_str(self->addr3); })
        .addProperty(
            "addr4",
            +[](const Owned<wifi::Frame> *self) { return mac_str(self->addr4); })
        .addProperty(
            "body",
            +[](const Owned<wifi::Frame> *self) { return bytes_of(self->body); })
        .endClass()
        // luabridge nests no class in a class: the Frame table hosts its enums like a namespace
        .beginNamespace("Frame")
        .beginNamespace("Type")
        .addVariable("management", wifi::Frame::Type::management)
        .addVariable("control", wifi::Frame::Type::control)
        .addVariable("data", wifi::Frame::Type::data)
        .addVariable("extension", wifi::Frame::Type::extension)
        .endNamespace()
        .beginNamespace("ManagementSubtype")
        .addVariable("assoc_request", static_cast<int>(wifi::Frame::ManagementSubtype::assoc_request))
        .addVariable("assoc_response", static_cast<int>(wifi::Frame::ManagementSubtype::assoc_response))
        .addVariable("reassoc_request", static_cast<int>(wifi::Frame::ManagementSubtype::reassoc_request))
        .addVariable("reassoc_response", static_cast<int>(wifi::Frame::ManagementSubtype::reassoc_response))
        .addVariable("probe_request", static_cast<int>(wifi::Frame::ManagementSubtype::probe_request))
        .addVariable("probe_response", static_cast<int>(wifi::Frame::ManagementSubtype::probe_response))
        .addVariable("timing_advertisement", static_cast<int>(wifi::Frame::ManagementSubtype::timing_advertisement))
        .addVariable("beacon", static_cast<int>(wifi::Frame::ManagementSubtype::beacon))
        .addVariable("atim", static_cast<int>(wifi::Frame::ManagementSubtype::atim))
        .addVariable("disassociation", static_cast<int>(wifi::Frame::ManagementSubtype::disassociation))
        .addVariable("authentication", static_cast<int>(wifi::Frame::ManagementSubtype::authentication))
        .addVariable("deauthentication", static_cast<int>(wifi::Frame::ManagementSubtype::deauthentication))
        .addVariable("action", static_cast<int>(wifi::Frame::ManagementSubtype::action))
        .addVariable("action_no_ack", static_cast<int>(wifi::Frame::ManagementSubtype::action_no_ack))
        .endNamespace()
        .beginNamespace("ControlSubtype")
        .addVariable("trigger", static_cast<int>(wifi::Frame::ControlSubtype::trigger))
        .addVariable("tack", static_cast<int>(wifi::Frame::ControlSubtype::tack))
        .addVariable("beamforming_report_poll", static_cast<int>(wifi::Frame::ControlSubtype::beamforming_report_poll))
        .addVariable("vht_ndp_announcement", static_cast<int>(wifi::Frame::ControlSubtype::vht_ndp_announcement))
        .addVariable("control_frame_extension", static_cast<int>(wifi::Frame::ControlSubtype::control_frame_extension))
        .addVariable("control_wrapper", static_cast<int>(wifi::Frame::ControlSubtype::control_wrapper))
        .addVariable("block_ack_request", static_cast<int>(wifi::Frame::ControlSubtype::block_ack_request))
        .addVariable("block_ack", static_cast<int>(wifi::Frame::ControlSubtype::block_ack))
        .addVariable("ps_poll", static_cast<int>(wifi::Frame::ControlSubtype::ps_poll))
        .addVariable("rts", static_cast<int>(wifi::Frame::ControlSubtype::rts))
        .addVariable("cts", static_cast<int>(wifi::Frame::ControlSubtype::cts))
        .addVariable("ack", static_cast<int>(wifi::Frame::ControlSubtype::ack))
        .addVariable("cf_end", static_cast<int>(wifi::Frame::ControlSubtype::cf_end))
        .addVariable("cf_end_ack", static_cast<int>(wifi::Frame::ControlSubtype::cf_end_ack))
        .endNamespace()
        .endNamespace()
        .addFunction("parse_frame", checked<+[](const std::string & data) {
                         return parse(data, &wifi::parse_frame);
                     }>)

        .beginClass<wifi::Addresses>("Addresses")
        .addConstructor<void (*)()>()
        .addProperty(
            "ra",
            +[](const wifi::Addresses *self) { return mac_str(self->ra); })
        .addProperty(
            "ta",
            +[](const wifi::Addresses *self) { return mac_str(self->ta); })
        .addProperty(
            "bssid",
            +[](const wifi::Addresses *self) { return mac_str(self->bssid); })
        .addProperty(
            "sa",
            +[](const wifi::Addresses *self) { return mac_str(self->sa); })
        .addProperty(
            "da",
            +[](const wifi::Addresses *self) { return mac_str(self->da); })
        .endClass()
        .addFunction(
            "addresses",
            +[](const Owned<wifi::Frame> & frame) { return wifi::addresses(frame); })
        .addFunction(
            "management_subtype",
            +[](const Owned<wifi::Frame> & frame) -> std::optional<int> {
                const std::optional<wifi::Frame::ManagementSubtype> subtype = wifi::management_subtype(frame);
                return subtype.has_value() ? std::optional<int>(static_cast<int>(*subtype)) : std::nullopt;
            })
        .addFunction(
            "control_subtype",
            +[](const Owned<wifi::Frame> & frame) -> std::optional<int> {
                const std::optional<wifi::Frame::ControlSubtype> subtype = wifi::control_subtype(frame);
                return subtype.has_value() ? std::optional<int>(static_cast<int>(*subtype)) : std::nullopt;
            })
        .addFunction("parse_mac", checked<+[](const std::string & mac) -> Result<std::string> {
                         Result<wifi::MacAddress> parsed = wifi::parse_mac(mac);
                         SIHD_UNEXPECTED_RETURN(parsed);
                         return bytes_of(ArrUByteView(parsed->data(), parsed->size()));
                     }>)
        .addFunction("is_group", checked<+[](const std::string & mac) {
                         return mac_flag(mac, &wifi::is_group);
                     }>)
        .addFunction("is_locally_administered", checked<+[](const std::string & mac) {
                         return mac_flag(mac, &wifi::is_locally_administered);
                     }>);
}

void bind_packet(lua_State *state)
{
    wifi_namespace(state)
        .beginClass<Owned<wifi::Packet>>("Packet")
        .addConstructor<void (*)()>()
        .addProperty(
            "radiotap",
            +[](const Owned<wifi::Packet> *self) -> std::optional<Owned<wifi::Radiotap>> {
                if (self->radiotap.has_value() == false)
                    return std::nullopt;
                return owned(*self->radiotap, self->buffer);
            })
        .addProperty(
            "frame",
            +[](const Owned<wifi::Packet> *self) { return owned(self->frame, self->buffer); })
        .endClass()
        .addFunction("dissect_radiotap", checked<+[](const std::string & data) {
                         return parse(data, &wifi::dissect_radiotap);
                     }>)
        .addFunction("dissect_80211", checked<+[](const std::string & data) {
                         return parse(data, &wifi::dissect_80211);
                     }>);
}

void bind_management(lua_State *state)
{
    wifi_namespace(state)
        .beginClass<Owned<wifi::Beacon>>("Beacon")
        .addConstructor<void (*)()>()
        .addProperty("timestamp_us", &wifi::Beacon::timestamp_us, false)
        .addProperty("interval_tu", &wifi::Beacon::interval_tu, false)
        .addProperty("capability", &wifi::Beacon::capability, false)
        .addProperty("ess", &wifi::Beacon::ess, false)
        .addProperty("ibss", &wifi::Beacon::ibss, false)
        .addProperty("privacy", &wifi::Beacon::privacy, false)
        .addProperty(
            "information_elements",
            +[](const Owned<wifi::Beacon> *self) { return bytes_of(self->information_elements); })
        .endClass()
        .beginClass<Owned<wifi::AssocRequest>>("AssocRequest")
        .addConstructor<void (*)()>()
        .addProperty("capability", &wifi::AssocRequest::capability, false)
        .addProperty("listen_interval", &wifi::AssocRequest::listen_interval, false)
        .addProperty(
            "information_elements",
            +[](const Owned<wifi::AssocRequest> *self) { return bytes_of(self->information_elements); })
        .endClass()
        .beginClass<Owned<wifi::ReassocRequest>>("ReassocRequest")
        .addConstructor<void (*)()>()
        .addProperty("capability", &wifi::ReassocRequest::capability, false)
        .addProperty("listen_interval", &wifi::ReassocRequest::listen_interval, false)
        .addProperty(
            "current_ap",
            +[](const Owned<wifi::ReassocRequest> *self) { return mac_str(self->current_ap); })
        .addProperty(
            "information_elements",
            +[](const Owned<wifi::ReassocRequest> *self) { return bytes_of(self->information_elements); })
        .endClass()
        .beginClass<Owned<wifi::AssocResponse>>("AssocResponse")
        .addConstructor<void (*)()>()
        .addProperty("capability", &wifi::AssocResponse::capability, false)
        .addProperty("status", &wifi::AssocResponse::status, false)
        .addProperty("aid", &wifi::AssocResponse::aid, false)
        .addProperty(
            "information_elements",
            +[](const Owned<wifi::AssocResponse> *self) { return bytes_of(self->information_elements); })
        .endClass()
        .beginClass<Owned<wifi::Authentication>>("Authentication")
        .addConstructor<void (*)()>()
        .addProperty("algorithm", &wifi::Authentication::algorithm, false)
        .addProperty("sequence", &wifi::Authentication::sequence, false)
        .addProperty("status", &wifi::Authentication::status, false)
        .addProperty(
            "information_elements",
            +[](const Owned<wifi::Authentication> *self) { return bytes_of(self->information_elements); })
        .endClass()
        .beginClass<Owned<wifi::Action>>("Action")
        .addConstructor<void (*)()>()
        .addProperty("category", &wifi::Action::category, false)
        .addProperty(
            "body",
            +[](const Owned<wifi::Action> *self) { return bytes_of(self->body); })
        .endClass()

        .addFunction("parse_beacon", checked<+[](const std::string & data) {
                         return parse(data, &wifi::parse_beacon);
                     }>)
        .addFunction("parse_probe_response", checked<+[](const std::string & data) {
                         return parse(data, &wifi::parse_probe_response);
                     }>)
        .addFunction("parse_assoc_request", checked<+[](const std::string & data) {
                         return parse(data, &wifi::parse_assoc_request);
                     }>)

        .addFunction("parse_reassoc_request", checked<+[](const std::string & data) {
                         return parse(data, &wifi::parse_reassoc_request);
                     }>)
        .addFunction("parse_assoc_response", checked<+[](const std::string & data) {
                         return parse(data, &wifi::parse_assoc_response);
                     }>)
        .addFunction("parse_authentication", checked<+[](const std::string & data) {
                         return parse(data, &wifi::parse_authentication);
                     }>)
        .addFunction("parse_reason", checked<+[](const std::string & data) {
                         return wifi::parse_reason(ArrUByteView(data));
                     }>)
        .addFunction("parse_action", checked<+[](const std::string & data) {
                         return parse(data, &wifi::parse_action);
                     }>);
}

void bind_data(lua_State *state)
{
    wifi_namespace(state)
        .beginClass<Owned<wifi::Llc>>("Llc")
        .addConstructor<void (*)()>()
        .addProperty("ethertype", &wifi::Llc::ethertype, false)
        .addProperty(
            "oui",
            +[](const Owned<wifi::Llc> *self) { return bytes_of(self->oui); })
        .addProperty(
            "payload",
            +[](const Owned<wifi::Llc> *self) { return bytes_of(self->payload); })
        .endClass()
        .addFunction("parse_llc_snap", checked<+[](const std::string & data) {
                         return parse(data, &wifi::parse_llc_snap);
                     }>);
}

void bind_ies(lua_State *state)
{
    wifi_namespace(state)
        .beginNamespace("ies")
        .beginClass<Owned<wifi::ies::Element>>("Element")
        .addConstructor<void (*)()>()
        .addProperty(
            "id",
            +[](const Owned<wifi::ies::Element> *self) { return static_cast<uint8_t>(self->id); })
        .addProperty(
            "data",
            +[](const Owned<wifi::ies::Element> *self) { return bytes_of(self->data); })
        .endClass()
        .beginNamespace("Id")
        .addVariable("ssid", static_cast<int>(wifi::ies::Id::ssid))
        .addVariable("rates", static_cast<int>(wifi::ies::Id::rates))
        .addVariable("ds_parameter", static_cast<int>(wifi::ies::Id::ds_parameter))
        .addVariable("tim", static_cast<int>(wifi::ies::Id::tim))
        .addVariable("country", static_cast<int>(wifi::ies::Id::country))
        .addVariable("power_capability", static_cast<int>(wifi::ies::Id::power_capability))
        .addVariable("ht_capabilities", static_cast<int>(wifi::ies::Id::ht_capabilities))
        .addVariable("rsn", static_cast<int>(wifi::ies::Id::rsn))
        .addVariable("extended_rates", static_cast<int>(wifi::ies::Id::extended_rates))
        .addVariable("ht_operation", static_cast<int>(wifi::ies::Id::ht_operation))
        .addVariable("extended_capabilities", static_cast<int>(wifi::ies::Id::extended_capabilities))
        .addVariable("vht_capabilities", static_cast<int>(wifi::ies::Id::vht_capabilities))
        .addVariable("vht_operation", static_cast<int>(wifi::ies::Id::vht_operation))
        .addVariable("vendor", static_cast<int>(wifi::ies::Id::vendor))
        .addVariable("extension", static_cast<int>(wifi::ies::Id::extension))
        .endNamespace()
        .beginClass<Owned<wifi::ies::WpsAttribute>>("WpsAttribute")
        .addConstructor<void (*)()>()
        .addProperty("id", &wifi::ies::WpsAttribute::id, false)
        .addProperty(
            "value",
            +[](const Owned<wifi::ies::WpsAttribute> *self) { return bytes_of(self->value); })
        .endClass()
        .beginClass<wifi::ies::Rate>("Rate")
        .addConstructor<void (*)()>()
        .addProperty("kbps", &wifi::ies::Rate::kbps, false)
        .addProperty("basic", &wifi::ies::Rate::basic, false)
        .endClass()
        .beginClass<wifi::ies::Suite>("Suite")
        .addConstructor<void (*)()>()
        .addProperty("type", &wifi::ies::Suite::type, false)
        .addProperty(
            "oui",
            +[](const wifi::ies::Suite *self) { return bytes_of(self->oui); })
        .endClass()
        .beginClass<wifi::ies::Rsn>("Rsn")
        .addConstructor<void (*)()>()
        .addProperty("version", &wifi::ies::Rsn::version, false)
        .addProperty("group", &wifi::ies::Rsn::group, false)
        .addProperty("pairwise", &wifi::ies::Rsn::pairwise, false)
        .addProperty("akms", &wifi::ies::Rsn::akms, false)
        .addProperty("capabilities", &wifi::ies::Rsn::capabilities, false)
        .endClass()
        .beginClass<wifi::ies::Wps>("Wps")
        .addConstructor<void (*)()>()
        .addProperty("version", &wifi::ies::Wps::version, false)
        .addProperty("setup_state", &wifi::ies::Wps::setup_state, false)
        .addProperty("ap_setup_locked", &wifi::ies::Wps::ap_setup_locked, false)
        .addProperty("manufacturer", &wifi::ies::Wps::manufacturer, false)
        .addProperty("model_name", &wifi::ies::Wps::model_name, false)
        .addProperty("model_number", &wifi::ies::Wps::model_number, false)
        .addProperty("device_name", &wifi::ies::Wps::device_name, false)
        .endClass()
        .addFunction("elements",
                     checked<+[](const std::string & data) -> Result<std::vector<Owned<wifi::ies::Element>>> {
                         const Buffer buffer = buffer_of(data);
                         Result<std::vector<wifi::ies::Element>> elements = wifi::ies::elements(ArrUByteView(*buffer));
                         SIHD_UNEXPECTED_RETURN(elements);
                         return owned_all(std::move(*elements), buffer);
                     }>)

        .addFunction("ssid", checked<+[](const std::string & data) {
                         return decode<std::string>(data, &wifi::ies::ssid);
                     }>)
        .addFunction("channel", checked<+[](const std::string & data) {
                         return decode<std::optional<uint8_t>>(data, &wifi::ies::channel);
                     }>)
        .addFunction("rates", checked<+[](const std::string & data) {
                         return decode<std::vector<wifi::ies::Rate>>(data, &wifi::ies::rates);
                     }>)
        .addFunction("rsn", checked<+[](const std::string & data) {
                         return decode<std::optional<wifi::ies::Rsn>>(data, &wifi::ies::rsn);
                     }>)
        .addFunction("wpa", checked<+[](const std::string & data) {
                         return decode<std::optional<wifi::ies::Rsn>>(data, &wifi::ies::wpa);
                     }>)
        .addFunction("wps", checked<+[](const std::string & data) {
                         return decode<std::optional<wifi::ies::Wps>>(data, &wifi::ies::wps);
                     }>)

        .addFunction(
            "find",
            checked<+[](const std::string & data, int id) -> Result<std::optional<Owned<wifi::ies::Element>>> {
                if (id < 0 || id > std::numeric_limits<uint8_t>::max())
                    return std::unexpected(Error(ErrorCode::invalid_argument, "an element id is 0 to 255"));
                return find_owned<wifi::ies::Element>(data, [id](const std::vector<wifi::ies::Element> & elements) {
                    return wifi::ies::find(elements, static_cast<wifi::ies::Id>(id));
                });
            }>)
        .addFunction(
            "find_vendor",
            checked<+[](const std::string & data, const std::string & oui, int type)
                        -> Result<std::optional<Owned<wifi::ies::Vendor>>> {
                wifi::Oui wanted = {};
                if (oui.size() != wanted.size() || type < 0 || type > std::numeric_limits<uint8_t>::max())
                    return std::unexpected(Error(ErrorCode::invalid_argument, "an oui is 3 bytes, a type 0 to 255"));
                std::ranges::copy(oui, wanted.begin());
                return find_owned<wifi::ies::Vendor>(
                    data,
                    [&wanted, type](const std::vector<wifi::ies::Element> & elements) {
                        return wifi::ies::find_vendor(elements, wanted, static_cast<uint8_t>(type));
                    });
            }>)
        .addFunction(
            "vendor_of",
            +[](const Owned<wifi::ies::Element> & element) -> std::optional<Owned<wifi::ies::Vendor>> {
                std::optional<wifi::ies::Vendor> vendor = wifi::ies::vendor_of(element);
                if (vendor.has_value() == false)
                    return std::nullopt;
                return owned(std::move(*vendor), element.buffer);
            })

        .addFunction("parse_rsn", checked<+[](const std::string & data) {
                         return wifi::ies::parse_rsn(ArrUByteView(data));
                     }>)
        .addFunction("parse_wps", checked<+[](const std::string & data) {
                         return wifi::ies::parse_wps(ArrUByteView(data));
                     }>)

        .addFunction("cipher_str", &wifi::ies::cipher_str)
        .addFunction("akm_str", &wifi::ies::akm_str)
        .endNamespace();
}

void bind_capabilities(lua_State *state)
{
    wifi_namespace(state)
        .beginNamespace("ies")
        .beginClass<wifi::ies::HtCapabilities>("HtCapabilities")
        .addConstructor<void (*)()>()
        .addProperty("info", &wifi::ies::HtCapabilities::info, false)
        .addProperty("ldpc", &wifi::ies::HtCapabilities::ldpc, false)
        .addProperty("width_40mhz", &wifi::ies::HtCapabilities::width_40mhz, false)
        .addProperty("spatial_multiplexing_power_save",
                     &wifi::ies::HtCapabilities::spatial_multiplexing_power_save,
                     false)
        .addProperty("greenfield", &wifi::ies::HtCapabilities::greenfield, false)
        .addProperty("short_gi_20mhz", &wifi::ies::HtCapabilities::short_gi_20mhz, false)
        .addProperty("short_gi_40mhz", &wifi::ies::HtCapabilities::short_gi_40mhz, false)
        .addProperty("tx_stbc", &wifi::ies::HtCapabilities::tx_stbc, false)
        .addProperty("rx_stbc_streams", &wifi::ies::HtCapabilities::rx_stbc_streams, false)
        .addProperty("max_amsdu_7935", &wifi::ies::HtCapabilities::max_amsdu_7935, false)
        .addProperty("dsss_cck_40mhz", &wifi::ies::HtCapabilities::dsss_cck_40mhz, false)
        .addProperty("forty_mhz_intolerant", &wifi::ies::HtCapabilities::forty_mhz_intolerant, false)
        .addProperty("ampdu_length_exponent", &wifi::ies::HtCapabilities::ampdu_length_exponent, false)
        .addProperty("ampdu_min_spacing", &wifi::ies::HtCapabilities::ampdu_min_spacing, false)
        .addProperty("rx_spatial_streams", &wifi::ies::HtCapabilities::rx_spatial_streams, false)
        .addProperty("rx_highest_rate_mbps", &wifi::ies::HtCapabilities::rx_highest_rate_mbps, false)
        .endClass()
        .beginClass<wifi::ies::VhtCapabilities>("VhtCapabilities")
        .addConstructor<void (*)()>()
        .addProperty("info", &wifi::ies::VhtCapabilities::info, false)
        .addProperty("max_mpdu_length", &wifi::ies::VhtCapabilities::max_mpdu_length, false)
        .addProperty("width_160mhz", &wifi::ies::VhtCapabilities::width_160mhz, false)
        .addProperty("width_80plus80mhz", &wifi::ies::VhtCapabilities::width_80plus80mhz, false)
        .addProperty("rx_ldpc", &wifi::ies::VhtCapabilities::rx_ldpc, false)
        .addProperty("short_gi_80mhz", &wifi::ies::VhtCapabilities::short_gi_80mhz, false)
        .addProperty("short_gi_160mhz", &wifi::ies::VhtCapabilities::short_gi_160mhz, false)
        .addProperty("tx_stbc", &wifi::ies::VhtCapabilities::tx_stbc, false)
        .addProperty("rx_stbc_streams", &wifi::ies::VhtCapabilities::rx_stbc_streams, false)
        .addProperty("su_beamformer", &wifi::ies::VhtCapabilities::su_beamformer, false)
        .addProperty("su_beamformee", &wifi::ies::VhtCapabilities::su_beamformee, false)
        .addProperty("mu_beamformer", &wifi::ies::VhtCapabilities::mu_beamformer, false)
        .addProperty("mu_beamformee", &wifi::ies::VhtCapabilities::mu_beamformee, false)
        .addProperty("ampdu_length_exponent", &wifi::ies::VhtCapabilities::ampdu_length_exponent, false)
        .addProperty("rx_mcs_map", &wifi::ies::VhtCapabilities::rx_mcs_map, false)
        .addProperty("tx_mcs_map", &wifi::ies::VhtCapabilities::tx_mcs_map, false)
        .addProperty("rx_spatial_streams", &wifi::ies::VhtCapabilities::rx_spatial_streams, false)
        .addProperty("tx_spatial_streams", &wifi::ies::VhtCapabilities::tx_spatial_streams, false)
        .endClass()
        .beginClass<wifi::ies::Standards>("Standards")
        .addConstructor<void (*)()>()
        .addProperty("ht", &wifi::ies::Standards::ht, false)
        .addProperty("vht", &wifi::ies::Standards::vht, false)
        .addProperty("he", &wifi::ies::Standards::he, false)
        .addProperty("eht", &wifi::ies::Standards::eht, false)
        .endClass()
        .beginClass<wifi::ies::Country>("Country")
        .addConstructor<void (*)()>()
        .addProperty("code", &wifi::ies::Country::code, false)
        .addProperty(
            "environment",
            +[](const wifi::ies::Country *self) { return std::string(1, self->environment); })
        .addProperty("channels", &wifi::ies::Country::channels, false)
        .endClass()
        // luabridge nests no class in a class: the Country table hosts Channels like a namespace
        .beginNamespace("Country")
        .beginClass<wifi::ies::Country::Channels>("Channels")
        .addConstructor<void (*)()>()
        .addProperty("first", &wifi::ies::Country::Channels::first, false)
        .addProperty("count", &wifi::ies::Country::Channels::count, false)
        .addProperty("max_power_dbm", &wifi::ies::Country::Channels::max_power_dbm, false)
        .endClass()
        .endNamespace()
        .beginClass<wifi::ies::Tim>("Tim")
        .addConstructor<void (*)()>()
        .addProperty("dtim_count", &wifi::ies::Tim::dtim_count, false)
        .addProperty("dtim_period", &wifi::ies::Tim::dtim_period, false)
        .addProperty("multicast_buffered", &wifi::ies::Tim::multicast_buffered, false)
        .endClass()
        .beginClass<Owned<wifi::ies::Vendor>>("Vendor")
        .addConstructor<void (*)()>()
        .addProperty("type", &wifi::ies::Vendor::type, false)
        .addProperty(
            "oui",
            +[](const Owned<wifi::ies::Vendor> *self) { return bytes_of(self->oui); })
        .addProperty(
            "data",
            +[](const Owned<wifi::ies::Vendor> *self) { return bytes_of(self->data); })
        .endClass()

        .addFunction("ht_capabilities", checked<+[](const std::string & data) {
                         return decode<std::optional<wifi::ies::HtCapabilities>>(data, &wifi::ies::ht_capabilities);
                     }>)
        .addFunction("vht_capabilities", checked<+[](const std::string & data) {
                         return decode<std::optional<wifi::ies::VhtCapabilities>>(data, &wifi::ies::vht_capabilities);
                     }>)
        .addFunction("country", checked<+[](const std::string & data) {
                         return decode<std::optional<wifi::ies::Country>>(data, &wifi::ies::country);
                     }>)
        .addFunction("tim", checked<+[](const std::string & data) {
                         return decode<std::optional<wifi::ies::Tim>>(data, &wifi::ies::tim);
                     }>)
        .addFunction("standards", checked<+[](const std::string & data) {
                         return decode<wifi::ies::Standards>(data, &wifi::ies::standards);
                     }>)

        .addFunction("vendors", checked<+[](const std::string & data) -> Result<std::vector<Owned<wifi::ies::Vendor>>> {
                         const Buffer buffer = buffer_of(data);
                         Result<std::vector<wifi::ies::Element>> elements = wifi::ies::elements(ArrUByteView(*buffer));
                         SIHD_UNEXPECTED_RETURN(elements);
                         return owned_all(wifi::ies::vendors(*elements), buffer);
                     }>)

        .addFunction(
            "readable_elements",
            +[](const std::string & data) {
                const Buffer buffer = buffer_of(data);
                return owned_all(wifi::ies::readable_elements(ArrUByteView(*buffer)), buffer);
            })
        .addFunction(
            "wps_attributes",
            +[](const std::string & data) {
                const Buffer buffer = buffer_of(data);
                return owned_all(wifi::ies::wps_attributes(ArrUByteView(*buffer)), buffer);
            })
        .endNamespace();
}

void bind_taxonomy(lua_State *state)
{
    wifi_namespace(state)
        .beginNamespace("taxonomy")
        .addFunction(
            "element_signature",
            +[](const std::string & data) { return wifi::taxonomy::element_signature(ArrUByteView(data)); })
        .addFunction("signature", checked<+[](const std::string & probe_request, const std::string & assoc_request) {
                         return wifi::taxonomy::signature(ArrUByteView(probe_request), ArrUByteView(assoc_request));
                     }>)
        .endNamespace();
}

} // namespace

void LuaWifiApi::load_all(Vm & vm)
{
    LuaWifiApi::load_base(vm);
}

void LuaWifiApi::load_base(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("wifi")
        .beginNamespace("Security")
        .addVariable("unknown", wifi::Security::unknown)
        .addVariable("open", wifi::Security::open)
        .addVariable("owe", wifi::Security::owe)
        .addVariable("wep", wifi::Security::wep)
        .addVariable("personal", wifi::Security::personal)
        .addVariable("enterprise", wifi::Security::enterprise)
        .endNamespace()
        .beginNamespace("Backend")
        .addVariable("network_manager", wifi::Backend::network_manager)
        .addVariable("iwd", wifi::Backend::iwd)
        .addVariable("wpa_supplicant", wifi::Backend::wpa_supplicant)
        .addVariable("wlan_service", wifi::Backend::wlan_service)
        .endNamespace()
        .beginNamespace("State")
        .addVariable("disconnected", wifi::State::disconnected)
        .addVariable("connecting", wifi::State::connecting)
        .addVariable("connected", wifi::State::connected)
        .addVariable("disconnecting", wifi::State::disconnecting)
        .endNamespace()

        .beginClass<wifi::Link>("Link")
        .addConstructor<void (*)()>()
        .addProperty("ssid", &wifi::Link::ssid, false)
        .addProperty("bssid", &wifi::Link::bssid, false)
        .addProperty("signal_dbm", &wifi::Link::signal_dbm, false)
        .addProperty("frequency_mhz", &wifi::Link::frequency_mhz, false)
        .addProperty("tx_bitrate_bps", &wifi::Link::tx_bitrate_bps, false)
        .endClass()
        .beginClass<wifi::AccessPoint>("AccessPoint")
        .addConstructor<void (*)()>()
        .addProperty("ssid", &wifi::AccessPoint::ssid, false)
        .addProperty("bssid", &wifi::AccessPoint::bssid, false)
        .addProperty("signal_dbm", &wifi::AccessPoint::signal_dbm, false)
        .addProperty("frequency_mhz", &wifi::AccessPoint::frequency_mhz, false)
        .addProperty("security", &wifi::AccessPoint::security, false)
        .addProperty("seen_ago", &wifi::AccessPoint::seen_ago, false)
        .addProperty("associated", &wifi::AccessPoint::associated, false)
        .endClass()
        .beginClass<wifi::KnownNetwork>("KnownNetwork")
        .addConstructor<void (*)()>()
        .addProperty("ssid", &wifi::KnownNetwork::ssid, false)
        .addProperty("security", &wifi::KnownNetwork::security, false)
        .endClass()
        .beginClass<LuaWifiMonitor>("Monitor")
        .addStaticFunction("open", checked<&LuaWifiMonitor::open>)
        .addFunction("set_run_timeout", checked<&LuaWifiMonitor::set_run_timeout>)
        .addFunction("run", &LuaWifiMonitor::run)
        .addFunction("events", &LuaWifiMonitor::events)
        .endClass()
        // luabridge nests no class in a class: the Monitor table hosts Event like a namespace
        .beginNamespace("Monitor")
        .beginClass<wifi::Monitor::Event>("Event")
        .addConstructor<void (*)()>()
        .addStaticProperty("Type",
                           [](lua_State *state) {
                               luabridge::LuaRef types = luabridge::newTable(state);
                               types["scan_done"] = wifi::Monitor::Event::Type::scan_done;
                               types["state_changed"] = wifi::Monitor::Event::Type::state_changed;
                               return types;
                           })
        .addProperty("type", &wifi::Monitor::Event::type, false)
        .addProperty("state", &wifi::Monitor::Event::state, false)
        .endClass()
        .endNamespace()

        .addFunction("link", checked<+[](lua_State *state, const std::string & interface_name) {
                         LuaGilRelease release(state);
                         return wifi::link(interface_name);
                     }>)
        .addFunction("access_points", checked<+[](lua_State *state, const std::string & interface_name) {
                         LuaGilRelease release(state);
                         return wifi::access_points(interface_name);
                     }>)
        .addFunction(
            "security_of",
            +[](const std::string & information_elements, bool privacy) {
                return wifi::security_of(ArrUByteView(information_elements), privacy);
            })
        .addFunction("scan",
                     checked<+[](lua_State *state, const std::string & interface_name, luabridge::LuaRef timeout)
                                 -> Result<void> {
                         const Result<Duration> scan_timeout = duration_of(timeout);
                         SIHD_UNEXPECTED_RETURN(scan_timeout);
                         LuaGilRelease release(state);
                         return wifi::scan(interface_name, *scan_timeout);
                     }>)

        .addFunction("backend", checked<+[](lua_State *state, const std::string & interface_name) {
                         LuaGilRelease release(state);
                         return wifi::backend(interface_name);
                     }>)
        .addFunction("state", checked<+[](lua_State *state, const std::string & interface_name) {
                         LuaGilRelease release(state);
                         return wifi::state(interface_name);
                     }>)
        .addFunction("connect", checked<&connect>)

        .addFunction("disconnect", checked<+[](lua_State *state, const std::string & interface_name) {
                         LuaGilRelease release(state);
                         return wifi::disconnect(interface_name);
                     }>)
        .addFunction("known_networks", checked<+[](lua_State *state, const std::string & interface_name) {
                         LuaGilRelease release(state);
                         return wifi::known_networks(interface_name);
                     }>)
        .addFunction("forget",
                     checked<+[](lua_State *state, const std::string & interface_name, const std::string & ssid) {
                         LuaGilRelease release(state);
                         return wifi::forget(interface_name, ssid);
                     }>)
        .endNamespace()
        .endNamespace();

    bind_radiotap(vm.lua_state());
    bind_frame(vm.lua_state());
    bind_packet(vm.lua_state());
    bind_management(vm.lua_state());
    bind_data(vm.lua_state());
    bind_ies(vm.lua_state());
    bind_capabilities(vm.lua_state());
    bind_taxonomy(vm.lua_state());
}

} // namespace sihd::lua
