#include <pybind11/pybind11.h>

#include <algorithm>
#include <iostream>

#include <sihd/py/PyApi.hpp>

#include "expected_caster.hpp"

namespace sihd::py
{

using namespace sihd::util;

namespace
{

pybind11::object & error_type()
{
    static pybind11::object type = pybind11::reinterpret_steal<pybind11::object>(
        PyErr_NewException("sihd.Error", PyExc_RuntimeError, nullptr));
    return type;
}

} // namespace

// args = (message,), code attribute = ErrorCode
void raise_error(const Error & error)
{
    // callers may release the GIL for the I/O (call_guard): raising touches the interpreter
    pybind11::gil_scoped_acquire acquire;
    // from the module, not a C++ static: libsihd_py.so and the sihd extension each have their own statics
    pybind11::object type = pybind11::module_::import("sihd").attr("Error");
    pybind11::object instance = type(error.message);
    instance.attr("code") = pybind11::cast(error.code);
    PyErr_SetObject(type.ptr(), instance.ptr());
    throw pybind11::error_already_set();
}

std::map<std::string_view, std::function<void(PyApi::PyModule &)>> & PyApi::api_lst()
{
    static std::map<std::string_view, std::function<void(PyModule &)>> instance;
    return instance;
}

PYBIND11_MODULE(sihd, m_sihd)
{
    m_sihd.doc() = "sihd";
    PyApi::set_api_to_module(m_sihd);
}

void PyApi::set_api_to_module(pybind11::module & m_sihd)
{
    m_sihd.attr("Error") = error_type();

    pybind11::enum_<ErrorCode>(m_sihd, "ErrorCode")
        .value("none", ErrorCode::none)
        .value("unknown", ErrorCode::unknown)
        .value("invalid_argument", ErrorCode::invalid_argument)
        .value("not_found", ErrorCode::not_found)
        .value("permission_denied", ErrorCode::permission_denied)
        .value("timeout", ErrorCode::timeout)
        .value("would_block", ErrorCode::would_block)
        .value("interrupted", ErrorCode::interrupted)
        .value("out_of_memory", ErrorCode::out_of_memory)
        .value("io_error", ErrorCode::io_error)
        .value("not_supported", ErrorCode::not_supported)
        .value("already_exists", ErrorCode::already_exists)
        .value("not_initialized", ErrorCode::not_initialized)
        .value("closed", ErrorCode::closed)
        .value("overflow", ErrorCode::overflow);

    PyModule pymodule(&m_sihd);

    for (const auto & pair : PyApi::api_lst())
    {
        pymodule.load(pair.first);
    }
}

void PyApi::add_api(std::string_view name, std::function<void(PyApi::PyModule &)> f)
{
    PyApi::api_lst()[name] = f;
}

PyApi::PyModule::PyModule(pybind11::module *module): _module_ptr(module) {}

PyApi::PyModule::~PyModule() = default;

pybind11::module & PyApi::PyModule::module() const
{
    return *_module_ptr;
}

bool PyApi::PyModule::is_loaded(std::string_view submodule_name)
{
    return std::find(_modules_imported.begin(), _modules_imported.end(), submodule_name) != _modules_imported.end();
}

bool PyApi::PyModule::load(std::string_view submodule_name)
{
    if (this->is_loaded(submodule_name))
        return true;
    auto it = PyApi::api_lst().find(submodule_name);
    if (it == PyApi::api_lst().end())
        return false;
    it->second(*this);
    _modules_imported.push_back(submodule_name);
    return true;
}

} // namespace sihd::py