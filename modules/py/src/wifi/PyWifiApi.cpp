#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>

#include <sihd/py/wifi/PyWifiApi.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/wifi/Monitor.hpp>
#include <sihd/wifi/connection.hpp>
#include <sihd/wifi/link.hpp>
#include <sihd/wifi/scan.hpp>

#include "../expected_caster.hpp"

namespace sihd::py
{

using namespace sihd::util;

namespace
{

wifi::Security security_of(const pybind11::bytes & information_elements, bool privacy)
{
    const std::string elements = information_elements;
    return wifi::security_of(ArrUByteView(elements), privacy);
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

    pybind11::class_<wifi::Link>(m_wifi, "Link")
        .def(pybind11::init<>())
        .def_readonly("ssid", &wifi::Link::ssid)
        .def_readonly("bssid", &wifi::Link::bssid)
        .def_readonly("signal_dbm", &wifi::Link::signal_dbm)
        .def_readonly("frequency_mhz", &wifi::Link::frequency_mhz)
        .def_readonly("tx_bitrate_bps", &wifi::Link::tx_bitrate_bps);
    pybind11::class_<wifi::AccessPoint>(m_wifi, "AccessPoint")
        .def(pybind11::init<>())
        .def_readonly("ssid", &wifi::AccessPoint::ssid)
        .def_readonly("bssid", &wifi::AccessPoint::bssid)
        .def_readonly("signal_dbm", &wifi::AccessPoint::signal_dbm)
        .def_readonly("frequency_mhz", &wifi::AccessPoint::frequency_mhz)
        .def_readonly("security", &wifi::AccessPoint::security)
        .def_readonly("seen_ago", &wifi::AccessPoint::seen_ago)
        .def_readonly("associated", &wifi::AccessPoint::associated);
    pybind11::class_<wifi::KnownNetwork>(m_wifi, "KnownNetwork")
        .def(pybind11::init<>())
        .def_readonly("ssid", &wifi::KnownNetwork::ssid)
        .def_readonly("security", &wifi::KnownNetwork::security);

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
}

namespace
{

void __attribute__((constructor)) premain()
{
    PyApi::add_api("sihd_wifi", &PyWifiApi::add_wifi_api);
}

} // namespace

} // namespace sihd::py
