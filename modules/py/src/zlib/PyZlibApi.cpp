#include <pybind11/pybind11.h>

#include <sihd/core/Device.hpp>
#include <sihd/py/zlib/PyZlibApi.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>
#include <sihd/zlib/DevDeflate.hpp>
#include <sihd/zlib/DevInflate.hpp>

namespace sihd::py
{

using namespace sihd::util;
using namespace sihd::zlib;
using sihd::core::Device;

void PyZlibApi::add_zlib_api(PyApi::PyModule & pymodule)
{
    auto m_sihd = pymodule.module();
    if (pymodule.load("sihd_core") == false)
        return;

    pybind11::module m_zlib = m_sihd.def_submodule("zlib", "sihd::zlib");

    pybind11::class_<DevDeflate, Device, SmartNodePtr<DevDeflate>>(m_zlib, "DevDeflate")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def("set_mode", &DevDeflate::set_mode)
        .def("set_level", &DevDeflate::set_level)
        .def("set_format", &DevDeflate::set_format);

    pybind11::class_<DevInflate, Device, SmartNodePtr<DevInflate>>(m_zlib, "DevInflate")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def("set_mode", &DevInflate::set_mode)
        .def("set_output_capacity", &DevInflate::set_output_capacity);
}

namespace
{

void __attribute__((constructor)) premain()
{
    PyApi::add_api("sihd_zlib", &PyZlibApi::add_zlib_api);
}

} // namespace

} // namespace sihd::py
