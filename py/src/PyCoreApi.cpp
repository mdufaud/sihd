#include <sihd/py/PyCoreApi.hpp>

namespace sihd::py
{

using namespace sihd::util;
using namespace sihd::core;

PYBIND11_MODULE(sihd_core, m)
{
    m.doc() = "hello";

    pybind11::object named = (pybind11::object) pybind11::module_::import("sihd_util").attr("Named");

    pybind11::class_<Channel> channel(m, "Channel", named);
    channel.def(pybind11::init<const std::string &, const std::string &, size_t, Node *>());
    channel.def(pybind11::init<const std::string &, const std::string &, Node *>());
    channel.def(pybind11::init<const std::string &, const std::string &, size_t>());
    channel.def(pybind11::init<const std::string &, const std::string &>());
}

}