#include <sihd/py/PyCoreApi.hpp>

namespace sihd::py
{

using namespace sihd::util;
using namespace sihd::core;

static void add_core_api(PyApi::PyModule & pymodule)
{
    auto m_sihd = pymodule.module();
    if (pymodule.load("sihd_util") == false)
        return ;
    pybind11::module m_util = (pybind11::module)m_sihd.attr("util");

    pybind11::module m_core = m_sihd.def_submodule("core", "sihd::core");

    pybind11::class_<Channel, Named, SmartNodePtr<Channel>> channel(m_core, "Channel");
    channel.def(pybind11::init<const std::string &, const std::string &, size_t, Node *>());
    channel.def(pybind11::init<const std::string &, const std::string &, Node *>());
    channel.def(pybind11::init<const std::string &, const std::string &, size_t>());
    channel.def(pybind11::init<const std::string &, const std::string &>());
}

static void __attribute__ ((constructor)) premain()
{
    PyApi::add_api("sihd_core", &add_core_api);
}

}