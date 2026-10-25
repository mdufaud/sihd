#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <sihd/net/ip.hpp>
#include <sihd/py/wifi/PyWifiApi.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/wifi/Monitor.hpp>
#include <sihd/wifi/capabilities.hpp>
#include <sihd/wifi/connection.hpp>
#include <sihd/wifi/data.hpp>
#include <sihd/wifi/dissect.hpp>
#include <sihd/wifi/frame.hpp>
#include <sihd/wifi/ies.hpp>
#include <sihd/wifi/link.hpp>
#include <sihd/wifi/management.hpp>
#include <sihd/wifi/radiotap.hpp>
#include <sihd/wifi/scan.hpp>
#include <sihd/wifi/taxonomy.hpp>

#include "../expected_caster.hpp"

namespace sihd::py
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

wifi::Security security_of(const pybind11::bytes & information_elements, bool privacy)
{
    const std::string elements = information_elements;
    return wifi::security_of(ArrUByteView(elements), privacy);
}

Buffer buffer_of(const pybind11::bytes & data)
{
    return std::make_shared<const std::string>(static_cast<std::string>(data));
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
Result<Owned<T>> parse(const pybind11::bytes & data, Result<T> (*parser)(ArrUByteView))
{
    const Buffer buffer = buffer_of(data);
    Result<T> parsed = parser(ArrUByteView(*buffer));
    SIHD_UNEXPECTED_RETURN(parsed);
    return owned(std::move(*parsed), buffer);
}

pybind11::bytes bytes_of(ArrUByteView view)
{
    return pybind11::bytes(std::string(view.begin(), view.end()));
}

pybind11::bytes bytes_of(const wifi::Oui & oui)
{
    return pybind11::bytes(std::string(oui.begin(), oui.end()));
}

pybind11::str text_of(const std::string & text)
{
    return pybind11::reinterpret_steal<pybind11::str>(
        PyUnicode_DecodeUTF8(text.data(), static_cast<Py_ssize_t>(text.size()), "replace"));
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

Result<wifi::Oui> oui_of(const pybind11::bytes & oui)
{
    const std::string text = oui;
    wifi::Oui result = {};
    if (text.size() != result.size())
        return std::unexpected(Error(ErrorCode::invalid_argument, "an oui is {} bytes", result.size()));
    std::ranges::copy(text, result.begin());
    return result;
}

Result<bool> mac_flag(const std::string & mac, bool (*test)(const wifi::MacAddress &))
{
    Result<wifi::MacAddress> parsed = wifi::parse_mac(mac);
    SIHD_UNEXPECTED_RETURN(parsed);
    return test(*parsed);
}

template <typename T>
void bind_ssid(pybind11::class_<T> & binding)
{
    binding.def_property(
        "ssid",
        [](const T & self) { return text_of(self.ssid); },
        [](T & self, const pybind11::bytes & ssid) { self.ssid = static_cast<std::string>(ssid); });
}

template <typename T, typename Finder>
Result<std::optional<Owned<T>>> find_owned(const pybind11::bytes & data, Finder finder)
{
    const Buffer buffer = buffer_of(data);
    Result<std::vector<wifi::ies::Element>> elements = wifi::ies::elements(ArrUByteView(*buffer));
    SIHD_UNEXPECTED_RETURN(elements);
    std::optional<T> found = finder(*elements);
    if (found.has_value() == false)
        return std::nullopt;
    return owned(std::move(*found), buffer);
}

template <typename T, typename Decoder>
Result<T> decode(const pybind11::bytes & data, Decoder decoder)
{
    const std::string buffer = data;
    Result<std::vector<wifi::ies::Element>> elements = wifi::ies::elements(ArrUByteView(buffer));
    SIHD_UNEXPECTED_RETURN(elements);
    return decoder(*elements);
}

void bind_radiotap(pybind11::module & wifi_module)
{
    pybind11::class_<Owned<wifi::Radiotap>> radiotap(wifi_module, "Radiotap");
    pybind11::class_<wifi::Radiotap::AntennaSignal>(radiotap, "AntennaSignal")
        .def(pybind11::init<>())
        .def_readonly("antenna", &wifi::Radiotap::AntennaSignal::antenna)
        .def_readonly("signal_dbm", &wifi::Radiotap::AntennaSignal::signal_dbm);
    pybind11::class_<wifi::Radiotap::Mcs>(radiotap, "Mcs")
        .def(pybind11::init<>())
        .def_readonly("index", &wifi::Radiotap::Mcs::index)
        .def_readonly("bandwidth_mhz", &wifi::Radiotap::Mcs::bandwidth_mhz)
        .def_readonly("short_gi", &wifi::Radiotap::Mcs::short_gi)
        .def_readonly("greenfield", &wifi::Radiotap::Mcs::greenfield)
        .def_readonly("ldpc", &wifi::Radiotap::Mcs::ldpc)
        .def_readonly("stbc_streams", &wifi::Radiotap::Mcs::stbc_streams)
        .def_readonly("rate_kbps", &wifi::Radiotap::Mcs::rate_kbps);
    radiotap.def(pybind11::init<>())
        .def_readonly("tsft_us", &wifi::Radiotap::tsft_us)
        .def_readonly("rate_kbps", &wifi::Radiotap::rate_kbps)
        .def_readonly("mcs", &wifi::Radiotap::mcs)
        .def_readonly("frequency_mhz", &wifi::Radiotap::frequency_mhz)
        .def_readonly("channel_flags", &wifi::Radiotap::channel_flags)
        .def_readonly("signal_dbm", &wifi::Radiotap::signal_dbm)
        .def_readonly("noise_dbm", &wifi::Radiotap::noise_dbm)
        .def_readonly("antenna", &wifi::Radiotap::antenna)
        .def_readonly("antenna_signals", &wifi::Radiotap::antenna_signals)
        .def_readonly("short_preamble", &wifi::Radiotap::short_preamble)
        .def_readonly("wep", &wifi::Radiotap::wep)
        .def_readonly("fragmented", &wifi::Radiotap::fragmented)
        .def_readonly("fcs_at_end", &wifi::Radiotap::fcs_at_end)
        .def_readonly("data_padding", &wifi::Radiotap::data_padding)
        .def_readonly("bad_fcs", &wifi::Radiotap::bad_fcs)
        .def_readonly("header_size", &wifi::Radiotap::header_size)
        .def_property_readonly("payload", [](const Owned<wifi::Radiotap> & self) { return bytes_of(self.payload); });
    wifi_module.def("parse_radiotap", [](const pybind11::bytes & data) { return parse(data, &wifi::parse_radiotap); });
}

void bind_frame(pybind11::module & wifi_module)
{
    pybind11::class_<Owned<wifi::Frame>> frame(wifi_module, "Frame");
    pybind11::enum_<wifi::Frame::Type>(frame, "Type")
        .value("management", wifi::Frame::Type::management)
        .value("control", wifi::Frame::Type::control)
        .value("data", wifi::Frame::Type::data)
        .value("extension", wifi::Frame::Type::extension);
    pybind11::enum_<wifi::Frame::ManagementSubtype>(frame, "ManagementSubtype")
        .value("assoc_request", wifi::Frame::ManagementSubtype::assoc_request)
        .value("assoc_response", wifi::Frame::ManagementSubtype::assoc_response)
        .value("reassoc_request", wifi::Frame::ManagementSubtype::reassoc_request)
        .value("reassoc_response", wifi::Frame::ManagementSubtype::reassoc_response)
        .value("probe_request", wifi::Frame::ManagementSubtype::probe_request)
        .value("probe_response", wifi::Frame::ManagementSubtype::probe_response)
        .value("timing_advertisement", wifi::Frame::ManagementSubtype::timing_advertisement)
        .value("beacon", wifi::Frame::ManagementSubtype::beacon)
        .value("atim", wifi::Frame::ManagementSubtype::atim)
        .value("disassociation", wifi::Frame::ManagementSubtype::disassociation)
        .value("authentication", wifi::Frame::ManagementSubtype::authentication)
        .value("deauthentication", wifi::Frame::ManagementSubtype::deauthentication)
        .value("action", wifi::Frame::ManagementSubtype::action)
        .value("action_no_ack", wifi::Frame::ManagementSubtype::action_no_ack);
    pybind11::enum_<wifi::Frame::ControlSubtype>(frame, "ControlSubtype")
        .value("trigger", wifi::Frame::ControlSubtype::trigger)
        .value("tack", wifi::Frame::ControlSubtype::tack)
        .value("beamforming_report_poll", wifi::Frame::ControlSubtype::beamforming_report_poll)
        .value("vht_ndp_announcement", wifi::Frame::ControlSubtype::vht_ndp_announcement)
        .value("control_frame_extension", wifi::Frame::ControlSubtype::control_frame_extension)
        .value("control_wrapper", wifi::Frame::ControlSubtype::control_wrapper)
        .value("block_ack_request", wifi::Frame::ControlSubtype::block_ack_request)
        .value("block_ack", wifi::Frame::ControlSubtype::block_ack)
        .value("ps_poll", wifi::Frame::ControlSubtype::ps_poll)
        .value("rts", wifi::Frame::ControlSubtype::rts)
        .value("cts", wifi::Frame::ControlSubtype::cts)
        .value("ack", wifi::Frame::ControlSubtype::ack)
        .value("cf_end", wifi::Frame::ControlSubtype::cf_end)
        .value("cf_end_ack", wifi::Frame::ControlSubtype::cf_end_ack);
    frame.def(pybind11::init<>())
        .def_readonly("type", &wifi::Frame::type)
        .def_readonly("subtype", &wifi::Frame::subtype)
        .def_readonly("to_ds", &wifi::Frame::to_ds)
        .def_readonly("from_ds", &wifi::Frame::from_ds)
        .def_readonly("more_fragments", &wifi::Frame::more_fragments)
        .def_readonly("retry", &wifi::Frame::retry)
        .def_readonly("power_management", &wifi::Frame::power_management)
        .def_readonly("more_data", &wifi::Frame::more_data)
        .def_readonly("protected_frame", &wifi::Frame::protected_frame)
        .def_readonly("order", &wifi::Frame::order)
        .def_readonly("duration", &wifi::Frame::duration)
        .def_readonly("sequence_number", &wifi::Frame::sequence_number)
        .def_readonly("fragment_number", &wifi::Frame::fragment_number)
        .def_readonly("qos_tid", &wifi::Frame::qos_tid)
        .def_readonly("amsdu", &wifi::Frame::amsdu)
        .def_readonly("ht_control", &wifi::Frame::ht_control)
        .def_readonly("header_size", &wifi::Frame::header_size)
        .def_property_readonly("addr1", [](const Owned<wifi::Frame> & self) { return mac_str(self.addr1); })
        .def_property_readonly("addr2", [](const Owned<wifi::Frame> & self) { return mac_str(self.addr2); })
        .def_property_readonly("addr3", [](const Owned<wifi::Frame> & self) { return mac_str(self.addr3); })
        .def_property_readonly("addr4", [](const Owned<wifi::Frame> & self) { return mac_str(self.addr4); })
        .def_property_readonly("body", [](const Owned<wifi::Frame> & self) { return bytes_of(self.body); });
    wifi_module.def("parse_frame", [](const pybind11::bytes & data) { return parse(data, &wifi::parse_frame); });

    pybind11::class_<wifi::Addresses>(wifi_module, "Addresses")
        .def(pybind11::init<>())
        .def_property_readonly("ra", [](const wifi::Addresses & self) { return mac_str(self.ra); })
        .def_property_readonly("ta", [](const wifi::Addresses & self) { return mac_str(self.ta); })
        .def_property_readonly("bssid", [](const wifi::Addresses & self) { return mac_str(self.bssid); })
        .def_property_readonly("sa", [](const wifi::Addresses & self) { return mac_str(self.sa); })
        .def_property_readonly("da", [](const wifi::Addresses & self) { return mac_str(self.da); });
    wifi_module.def("addresses", [](const Owned<wifi::Frame> & frame) { return wifi::addresses(frame); });
    wifi_module.def("management_subtype",
                    [](const Owned<wifi::Frame> & frame) { return wifi::management_subtype(frame); });
    wifi_module.def("control_subtype", [](const Owned<wifi::Frame> & frame) { return wifi::control_subtype(frame); });
    wifi_module.def("parse_mac", [](const std::string & mac) -> Result<pybind11::bytes> {
        Result<wifi::MacAddress> parsed = wifi::parse_mac(mac);
        SIHD_UNEXPECTED_RETURN(parsed);
        return bytes_of(ArrUByteView(parsed->data(), parsed->size()));
    });
    wifi_module.def("is_group", [](const std::string & mac) { return mac_flag(mac, &wifi::is_group); });
    wifi_module.def("is_locally_administered",
                    [](const std::string & mac) { return mac_flag(mac, &wifi::is_locally_administered); });
}

void bind_packet(pybind11::module & wifi_module)
{
    pybind11::class_<Owned<wifi::Packet>>(wifi_module, "Packet")
        .def(pybind11::init<>())
        .def_property_readonly("radiotap",
                               [](const Owned<wifi::Packet> & self) -> std::optional<Owned<wifi::Radiotap>> {
                                   if (self.radiotap.has_value() == false)
                                       return std::nullopt;
                                   return owned(*self.radiotap, self.buffer);
                               })
        .def_property_readonly("frame",
                               [](const Owned<wifi::Packet> & self) { return owned(self.frame, self.buffer); });
    wifi_module.def("dissect_radiotap",
                    [](const pybind11::bytes & data) { return parse(data, &wifi::dissect_radiotap); });
    wifi_module.def("dissect_80211", [](const pybind11::bytes & data) { return parse(data, &wifi::dissect_80211); });
}

void bind_management(pybind11::module & wifi_module)
{
    pybind11::class_<Owned<wifi::Beacon>>(wifi_module, "Beacon")
        .def(pybind11::init<>())
        .def_readonly("timestamp_us", &wifi::Beacon::timestamp_us)
        .def_readonly("interval_tu", &wifi::Beacon::interval_tu)
        .def_readonly("capability", &wifi::Beacon::capability)
        .def_readonly("ess", &wifi::Beacon::ess)
        .def_readonly("ibss", &wifi::Beacon::ibss)
        .def_readonly("privacy", &wifi::Beacon::privacy)
        .def_property_readonly("information_elements",
                               [](const Owned<wifi::Beacon> & self) { return bytes_of(self.information_elements); });
    pybind11::class_<Owned<wifi::AssocRequest>>(wifi_module, "AssocRequest")
        .def(pybind11::init<>())
        .def_readonly("capability", &wifi::AssocRequest::capability)
        .def_readonly("listen_interval", &wifi::AssocRequest::listen_interval)
        .def_property_readonly("information_elements", [](const Owned<wifi::AssocRequest> & self) {
            return bytes_of(self.information_elements);
        });
    pybind11::class_<Owned<wifi::ReassocRequest>>(wifi_module, "ReassocRequest")
        .def(pybind11::init<>())
        .def_readonly("capability", &wifi::ReassocRequest::capability)
        .def_readonly("listen_interval", &wifi::ReassocRequest::listen_interval)
        .def_property_readonly("current_ap",
                               [](const Owned<wifi::ReassocRequest> & self) { return mac_str(self.current_ap); })
        .def_property_readonly("information_elements", [](const Owned<wifi::ReassocRequest> & self) {
            return bytes_of(self.information_elements);
        });
    pybind11::class_<Owned<wifi::AssocResponse>>(wifi_module, "AssocResponse")
        .def(pybind11::init<>())
        .def_readonly("capability", &wifi::AssocResponse::capability)
        .def_readonly("status", &wifi::AssocResponse::status)
        .def_readonly("aid", &wifi::AssocResponse::aid)
        .def_property_readonly("information_elements", [](const Owned<wifi::AssocResponse> & self) {
            return bytes_of(self.information_elements);
        });
    pybind11::class_<Owned<wifi::Authentication>>(wifi_module, "Authentication")
        .def(pybind11::init<>())
        .def_readonly("algorithm", &wifi::Authentication::algorithm)
        .def_readonly("sequence", &wifi::Authentication::sequence)
        .def_readonly("status", &wifi::Authentication::status)
        .def_property_readonly("information_elements", [](const Owned<wifi::Authentication> & self) {
            return bytes_of(self.information_elements);
        });
    pybind11::class_<Owned<wifi::Action>>(wifi_module, "Action")
        .def(pybind11::init<>())
        .def_readonly("category", &wifi::Action::category)
        .def_property_readonly("body", [](const Owned<wifi::Action> & self) { return bytes_of(self.body); });

    wifi_module.def("parse_beacon", [](const pybind11::bytes & data) { return parse(data, &wifi::parse_beacon); });
    wifi_module.def("parse_probe_response",
                    [](const pybind11::bytes & data) { return parse(data, &wifi::parse_probe_response); });
    wifi_module.def("parse_assoc_request",
                    [](const pybind11::bytes & data) { return parse(data, &wifi::parse_assoc_request); });
    wifi_module.def("parse_reassoc_request",
                    [](const pybind11::bytes & data) { return parse(data, &wifi::parse_reassoc_request); });
    wifi_module.def("parse_assoc_response",
                    [](const pybind11::bytes & data) { return parse(data, &wifi::parse_assoc_response); });
    wifi_module.def("parse_authentication",
                    [](const pybind11::bytes & data) { return parse(data, &wifi::parse_authentication); });
    wifi_module.def("parse_reason", [](const pybind11::bytes & data) {
        const std::string buffer = data;
        return wifi::parse_reason(ArrUByteView(buffer));
    });
    wifi_module.def("parse_action", [](const pybind11::bytes & data) { return parse(data, &wifi::parse_action); });
}

void bind_data(pybind11::module & wifi_module)
{
    pybind11::class_<Owned<wifi::Llc>>(wifi_module, "Llc")
        .def(pybind11::init<>())
        .def_readonly("ethertype", &wifi::Llc::ethertype)
        .def_property_readonly("oui", [](const Owned<wifi::Llc> & self) { return bytes_of(self.oui); })
        .def_property_readonly("payload", [](const Owned<wifi::Llc> & self) { return bytes_of(self.payload); });
    wifi_module.def("parse_llc_snap", [](const pybind11::bytes & data) { return parse(data, &wifi::parse_llc_snap); });
}

void bind_ies(pybind11::module & wifi_module)
{
    pybind11::module ies_module = wifi_module.def_submodule("ies", "sihd::wifi::ies");

    pybind11::enum_<wifi::ies::Id>(ies_module, "Id")
        .value("ssid", wifi::ies::Id::ssid)
        .value("rates", wifi::ies::Id::rates)
        .value("ds_parameter", wifi::ies::Id::ds_parameter)
        .value("tim", wifi::ies::Id::tim)
        .value("country", wifi::ies::Id::country)
        .value("power_capability", wifi::ies::Id::power_capability)
        .value("ht_capabilities", wifi::ies::Id::ht_capabilities)
        .value("rsn", wifi::ies::Id::rsn)
        .value("extended_rates", wifi::ies::Id::extended_rates)
        .value("ht_operation", wifi::ies::Id::ht_operation)
        .value("extended_capabilities", wifi::ies::Id::extended_capabilities)
        .value("vht_capabilities", wifi::ies::Id::vht_capabilities)
        .value("vht_operation", wifi::ies::Id::vht_operation)
        .value("vendor", wifi::ies::Id::vendor)
        .value("extension", wifi::ies::Id::extension);
    pybind11::class_<Owned<wifi::ies::Element>>(ies_module, "Element")
        .def(pybind11::init<>())
        .def_property_readonly("id",
                               [](const Owned<wifi::ies::Element> & self) { return static_cast<uint8_t>(self.id); })
        .def_property_readonly("data", [](const Owned<wifi::ies::Element> & self) { return bytes_of(self.data); });
    pybind11::class_<wifi::ies::Rate>(ies_module, "Rate")
        .def(pybind11::init<>())
        .def_readonly("kbps", &wifi::ies::Rate::kbps)
        .def_readonly("basic", &wifi::ies::Rate::basic);
    pybind11::class_<wifi::ies::Suite>(ies_module, "Suite")
        .def(pybind11::init<>())
        .def_readonly("type", &wifi::ies::Suite::type)
        .def_property_readonly("oui", [](const wifi::ies::Suite & self) { return bytes_of(self.oui); });
    pybind11::class_<wifi::ies::Rsn>(ies_module, "Rsn")
        .def(pybind11::init<>())
        .def_readonly("version", &wifi::ies::Rsn::version)
        .def_readonly("group", &wifi::ies::Rsn::group)
        .def_readonly("pairwise", &wifi::ies::Rsn::pairwise)
        .def_readonly("akms", &wifi::ies::Rsn::akms)
        .def_readonly("capabilities", &wifi::ies::Rsn::capabilities);
    pybind11::class_<Owned<wifi::ies::WpsAttribute>>(ies_module, "WpsAttribute")
        .def(pybind11::init<>())
        .def_readonly("id", &wifi::ies::WpsAttribute::id)
        .def_property_readonly("value",
                               [](const Owned<wifi::ies::WpsAttribute> & self) { return bytes_of(self.value); });
    pybind11::class_<wifi::ies::Wps>(ies_module, "Wps")
        .def(pybind11::init<>())
        .def_readonly("version", &wifi::ies::Wps::version)
        .def_readonly("setup_state", &wifi::ies::Wps::setup_state)
        .def_readonly("ap_setup_locked", &wifi::ies::Wps::ap_setup_locked)
        .def_property_readonly("manufacturer", [](const wifi::ies::Wps & self) { return text_of(self.manufacturer); })
        .def_property_readonly("model_name", [](const wifi::ies::Wps & self) { return text_of(self.model_name); })
        .def_property_readonly("model_number", [](const wifi::ies::Wps & self) { return text_of(self.model_number); })
        .def_property_readonly("device_name", [](const wifi::ies::Wps & self) { return text_of(self.device_name); });

    ies_module.def("elements", [](const pybind11::bytes & data) -> Result<std::vector<Owned<wifi::ies::Element>>> {
        const Buffer buffer = buffer_of(data);
        Result<std::vector<wifi::ies::Element>> elements = wifi::ies::elements(ArrUByteView(*buffer));
        SIHD_UNEXPECTED_RETURN(elements);
        return owned_all(std::move(*elements), buffer);
    });

    ies_module.def("ssid", [](const pybind11::bytes & data) -> Result<pybind11::str> {
        Result<std::string> ssid = decode<std::string>(data, &wifi::ies::ssid);
        SIHD_UNEXPECTED_RETURN(ssid);
        return text_of(*ssid);
    });
    ies_module.def("channel", [](const pybind11::bytes & data) {
        return decode<std::optional<uint8_t>>(data, &wifi::ies::channel);
    });
    ies_module.def("rates", [](const pybind11::bytes & data) {
        return decode<std::vector<wifi::ies::Rate>>(data, &wifi::ies::rates);
    });

    ies_module.def("rsn", [](const pybind11::bytes & data) {
        return decode<std::optional<wifi::ies::Rsn>>(data, &wifi::ies::rsn);
    });
    ies_module.def("wpa", [](const pybind11::bytes & data) {
        return decode<std::optional<wifi::ies::Rsn>>(data, &wifi::ies::wpa);
    });
    ies_module.def("wps", [](const pybind11::bytes & data) {
        return decode<std::optional<wifi::ies::Wps>>(data, &wifi::ies::wps);
    });

    ies_module.def("find", [](const pybind11::bytes & data, wifi::ies::Id id) {
        return find_owned<wifi::ies::Element>(data, [id](const std::vector<wifi::ies::Element> & elements) {
            return wifi::ies::find(elements, id);
        });
    });
    ies_module.def("find_vendor",
                   [](const pybind11::bytes & data,
                      const pybind11::bytes & oui,
                      uint8_t type) -> Result<std::optional<Owned<wifi::ies::Vendor>>> {
                       Result<wifi::Oui> parsed_oui = oui_of(oui);
                       SIHD_UNEXPECTED_RETURN(parsed_oui);
                       return find_owned<wifi::ies::Vendor>(
                           data,
                           [&parsed_oui, type](const std::vector<wifi::ies::Element> & elements) {
                               return wifi::ies::find_vendor(elements, *parsed_oui, type);
                           });
                   });
    ies_module.def("vendor_of",
                   [](const Owned<wifi::ies::Element> & element) -> std::optional<Owned<wifi::ies::Vendor>> {
                       std::optional<wifi::ies::Vendor> vendor = wifi::ies::vendor_of(element);
                       if (vendor.has_value() == false)
                           return std::nullopt;
                       return owned(std::move(*vendor), element.buffer);
                   });

    ies_module.def("parse_rsn", [](const pybind11::bytes & data) {
        const std::string body = data;
        return wifi::ies::parse_rsn(ArrUByteView(body));
    });
    ies_module.def("parse_wps", [](const pybind11::bytes & data) {
        const std::string attributes = data;
        return wifi::ies::parse_wps(ArrUByteView(attributes));
    });
    ies_module.def("cipher_str", &wifi::ies::cipher_str);
    ies_module.def("akm_str", &wifi::ies::akm_str);
}

void bind_capabilities(pybind11::module & wifi_module)
{
    pybind11::module ies_module = wifi_module.attr("ies");

    pybind11::class_<wifi::ies::HtCapabilities>(ies_module, "HtCapabilities")
        .def(pybind11::init<>())
        .def_readonly("info", &wifi::ies::HtCapabilities::info)
        .def_readonly("ldpc", &wifi::ies::HtCapabilities::ldpc)
        .def_readonly("width_40mhz", &wifi::ies::HtCapabilities::width_40mhz)
        .def_readonly("spatial_multiplexing_power_save", &wifi::ies::HtCapabilities::spatial_multiplexing_power_save)
        .def_readonly("greenfield", &wifi::ies::HtCapabilities::greenfield)
        .def_readonly("short_gi_20mhz", &wifi::ies::HtCapabilities::short_gi_20mhz)
        .def_readonly("short_gi_40mhz", &wifi::ies::HtCapabilities::short_gi_40mhz)
        .def_readonly("tx_stbc", &wifi::ies::HtCapabilities::tx_stbc)
        .def_readonly("rx_stbc_streams", &wifi::ies::HtCapabilities::rx_stbc_streams)
        .def_readonly("max_amsdu_7935", &wifi::ies::HtCapabilities::max_amsdu_7935)
        .def_readonly("dsss_cck_40mhz", &wifi::ies::HtCapabilities::dsss_cck_40mhz)
        .def_readonly("forty_mhz_intolerant", &wifi::ies::HtCapabilities::forty_mhz_intolerant)
        .def_readonly("ampdu_length_exponent", &wifi::ies::HtCapabilities::ampdu_length_exponent)
        .def_readonly("ampdu_min_spacing", &wifi::ies::HtCapabilities::ampdu_min_spacing)
        .def_readonly("rx_spatial_streams", &wifi::ies::HtCapabilities::rx_spatial_streams)
        .def_readonly("rx_highest_rate_mbps", &wifi::ies::HtCapabilities::rx_highest_rate_mbps);
    pybind11::class_<wifi::ies::VhtCapabilities>(ies_module, "VhtCapabilities")
        .def(pybind11::init<>())
        .def_readonly("info", &wifi::ies::VhtCapabilities::info)
        .def_readonly("max_mpdu_length", &wifi::ies::VhtCapabilities::max_mpdu_length)
        .def_readonly("width_160mhz", &wifi::ies::VhtCapabilities::width_160mhz)
        .def_readonly("width_80plus80mhz", &wifi::ies::VhtCapabilities::width_80plus80mhz)
        .def_readonly("rx_ldpc", &wifi::ies::VhtCapabilities::rx_ldpc)
        .def_readonly("short_gi_80mhz", &wifi::ies::VhtCapabilities::short_gi_80mhz)
        .def_readonly("short_gi_160mhz", &wifi::ies::VhtCapabilities::short_gi_160mhz)
        .def_readonly("tx_stbc", &wifi::ies::VhtCapabilities::tx_stbc)
        .def_readonly("rx_stbc_streams", &wifi::ies::VhtCapabilities::rx_stbc_streams)
        .def_readonly("su_beamformer", &wifi::ies::VhtCapabilities::su_beamformer)
        .def_readonly("su_beamformee", &wifi::ies::VhtCapabilities::su_beamformee)
        .def_readonly("mu_beamformer", &wifi::ies::VhtCapabilities::mu_beamformer)
        .def_readonly("mu_beamformee", &wifi::ies::VhtCapabilities::mu_beamformee)
        .def_readonly("ampdu_length_exponent", &wifi::ies::VhtCapabilities::ampdu_length_exponent)
        .def_readonly("rx_mcs_map", &wifi::ies::VhtCapabilities::rx_mcs_map)
        .def_readonly("tx_mcs_map", &wifi::ies::VhtCapabilities::tx_mcs_map)
        .def_readonly("rx_spatial_streams", &wifi::ies::VhtCapabilities::rx_spatial_streams)
        .def_readonly("tx_spatial_streams", &wifi::ies::VhtCapabilities::tx_spatial_streams);
    pybind11::class_<wifi::ies::Standards>(ies_module, "Standards")
        .def(pybind11::init<>())
        .def_readonly("ht", &wifi::ies::Standards::ht)
        .def_readonly("vht", &wifi::ies::Standards::vht)
        .def_readonly("he", &wifi::ies::Standards::he)
        .def_readonly("eht", &wifi::ies::Standards::eht);
    pybind11::class_<wifi::ies::Country> country(ies_module, "Country");
    pybind11::class_<wifi::ies::Country::Channels>(country, "Channels")
        .def(pybind11::init<>())
        .def_readonly("first", &wifi::ies::Country::Channels::first)
        .def_readonly("count", &wifi::ies::Country::Channels::count)
        .def_readonly("max_power_dbm", &wifi::ies::Country::Channels::max_power_dbm);
    country.def(pybind11::init<>())
        .def_property_readonly("code", [](const wifi::ies::Country & self) { return text_of(self.code); })
        .def_property_readonly(
            "environment",
            [](const wifi::ies::Country & self) { return text_of(std::string(1, self.environment)); })
        .def_readonly("channels", &wifi::ies::Country::channels);
    pybind11::class_<wifi::ies::Tim>(ies_module, "Tim")
        .def(pybind11::init<>())
        .def_readonly("dtim_count", &wifi::ies::Tim::dtim_count)
        .def_readonly("dtim_period", &wifi::ies::Tim::dtim_period)
        .def_readonly("multicast_buffered", &wifi::ies::Tim::multicast_buffered);
    pybind11::class_<Owned<wifi::ies::Vendor>>(ies_module, "Vendor")
        .def(pybind11::init<>())
        .def_readonly("type", &wifi::ies::Vendor::type)
        .def_property_readonly("oui", [](const Owned<wifi::ies::Vendor> & self) { return bytes_of(self.oui); })
        .def_property_readonly("data", [](const Owned<wifi::ies::Vendor> & self) { return bytes_of(self.data); });

    ies_module.def("ht_capabilities", [](const pybind11::bytes & data) {
        return decode<std::optional<wifi::ies::HtCapabilities>>(data, &wifi::ies::ht_capabilities);
    });
    ies_module.def("vht_capabilities", [](const pybind11::bytes & data) {
        return decode<std::optional<wifi::ies::VhtCapabilities>>(data, &wifi::ies::vht_capabilities);
    });
    ies_module.def("country", [](const pybind11::bytes & data) {
        return decode<std::optional<wifi::ies::Country>>(data, &wifi::ies::country);
    });
    ies_module.def("tim", [](const pybind11::bytes & data) {
        return decode<std::optional<wifi::ies::Tim>>(data, &wifi::ies::tim);
    });
    ies_module.def("standards", [](const pybind11::bytes & data) {
        return decode<wifi::ies::Standards>(data, &wifi::ies::standards);
    });

    ies_module.def("vendors", [](const pybind11::bytes & data) -> Result<std::vector<Owned<wifi::ies::Vendor>>> {
        const Buffer buffer = buffer_of(data);
        Result<std::vector<wifi::ies::Element>> elements = wifi::ies::elements(ArrUByteView(*buffer));
        SIHD_UNEXPECTED_RETURN(elements);
        return owned_all(wifi::ies::vendors(*elements), buffer);
    });
    ies_module.def("readable_elements", [](const pybind11::bytes & data) {
        const Buffer buffer = buffer_of(data);
        return owned_all(wifi::ies::readable_elements(ArrUByteView(*buffer)), buffer);
    });
    ies_module.def("wps_attributes", [](const pybind11::bytes & data) {
        const Buffer buffer = buffer_of(data);
        return owned_all(wifi::ies::wps_attributes(ArrUByteView(*buffer)), buffer);
    });
}

void bind_taxonomy(pybind11::module & wifi_module)
{
    pybind11::module taxonomy_module = wifi_module.def_submodule("taxonomy", "sihd::wifi::taxonomy");

    taxonomy_module.def("element_signature", [](const pybind11::bytes & data) {
        const std::string elements = data;
        return wifi::taxonomy::element_signature(ArrUByteView(elements));
    });
    taxonomy_module.def("signature", [](const pybind11::bytes & probe_request, const pybind11::bytes & assoc_request) {
        const std::string probe = probe_request;
        const std::string assoc = assoc_request;
        return wifi::taxonomy::signature(ArrUByteView(probe), ArrUByteView(assoc));
    });
}

} // namespace

void PyWifiApi::add_wifi_api(PyApi::PyModule & pymodule)
{
    auto m_sihd = pymodule.module();
    if (pymodule.load("sihd_util") == false)
        return;

    pybind11::module m_wifi = m_sihd.def_submodule("wifi", "sihd::wifi");
    pybind11::enum_<wifi::Security>(m_wifi, "Security")
        .value("unknown", wifi::Security::unknown)
        .value("open", wifi::Security::open)
        .value("owe", wifi::Security::owe)
        .value("wep", wifi::Security::wep)
        .value("personal", wifi::Security::personal)
        .value("enterprise", wifi::Security::enterprise);
    pybind11::enum_<wifi::Backend>(m_wifi, "Backend")
        .value("network_manager", wifi::Backend::network_manager)
        .value("iwd", wifi::Backend::iwd)
        .value("wpa_supplicant", wifi::Backend::wpa_supplicant)
        .value("wlan_service", wifi::Backend::wlan_service);
    pybind11::enum_<wifi::State>(m_wifi, "State")
        .value("disconnected", wifi::State::disconnected)
        .value("connecting", wifi::State::connecting)
        .value("connected", wifi::State::connected)
        .value("disconnecting", wifi::State::disconnecting);

    pybind11::class_<wifi::Link> link(m_wifi, "Link");
    pybind11::class_<wifi::AccessPoint> access_point(m_wifi, "AccessPoint");
    pybind11::class_<wifi::KnownNetwork> known_network(m_wifi, "KnownNetwork");
    bind_ssid(link);
    bind_ssid(access_point);
    bind_ssid(known_network);
    link.def(pybind11::init<>())
        .def_readonly("bssid", &wifi::Link::bssid)
        .def_readonly("signal_dbm", &wifi::Link::signal_dbm)
        .def_readonly("frequency_mhz", &wifi::Link::frequency_mhz)
        .def_readonly("tx_bitrate_bps", &wifi::Link::tx_bitrate_bps);
    access_point.def(pybind11::init<>())
        .def_readonly("bssid", &wifi::AccessPoint::bssid)
        .def_readonly("signal_dbm", &wifi::AccessPoint::signal_dbm)
        .def_readonly("frequency_mhz", &wifi::AccessPoint::frequency_mhz)
        .def_readonly("security", &wifi::AccessPoint::security)
        .def_readonly("seen_ago", &wifi::AccessPoint::seen_ago)
        .def_readonly("associated", &wifi::AccessPoint::associated);
    known_network.def(pybind11::init<>()).def_readonly("security", &wifi::KnownNetwork::security);

    const auto release_gil = pybind11::call_guard<pybind11::gil_scoped_release>();
    m_wifi.def("link", &wifi::link, release_gil);
    m_wifi.def("access_points", &wifi::access_points, release_gil);
    m_wifi.def("security_of", &security_of);
    m_wifi.def("scan", &wifi::scan, release_gil);
    m_wifi.def("backend", &wifi::backend, release_gil);
    m_wifi.def("state", &wifi::state, release_gil);
    m_wifi.def("connect",
               pybind11::overload_cast<const std::string &, const std::string &, Duration>(&wifi::connect),
               release_gil);
    m_wifi.def("connect",
               pybind11::overload_cast<const std::string &, const std::string &, const std::string &, Duration>(
                   &wifi::connect),
               release_gil);
    m_wifi.def("disconnect", &wifi::disconnect, release_gil);
    m_wifi.def("known_networks", &wifi::known_networks, release_gil);
    m_wifi.def("forget", &wifi::forget, release_gil);

    pybind11::class_<wifi::Monitor> monitor(m_wifi, "Monitor");
    pybind11::class_<wifi::Monitor::Event> event(monitor, "Event");
    pybind11::enum_<wifi::Monitor::Event::Type>(event, "Type")
        .value("scan_done", wifi::Monitor::Event::Type::scan_done)
        .value("state_changed", wifi::Monitor::Event::Type::state_changed);
    event.def(pybind11::init<>())
        .def_readonly("type", &wifi::Monitor::Event::type)
        .def_readonly("state", &wifi::Monitor::Event::state);
    monitor.def(pybind11::init<>())
        .def(pybind11::init<Duration>(), pybind11::arg("run_timeout"))
        .def("open", &wifi::Monitor::open, release_gil)
        .def("set_run_timeout", &wifi::Monitor::set_run_timeout)
        .def("run", &wifi::Monitor::run, release_gil)
        .def("events", &wifi::Monitor::events);

    bind_radiotap(m_wifi);
    bind_frame(m_wifi);
    bind_packet(m_wifi);
    bind_management(m_wifi);
    bind_data(m_wifi);
    bind_ies(m_wifi);
    bind_capabilities(m_wifi);
    bind_taxonomy(m_wifi);
}

namespace
{

void __attribute__((constructor)) premain()
{
    PyApi::add_api("sihd_wifi", &PyWifiApi::add_wifi_api);
}

} // namespace

} // namespace sihd::py
