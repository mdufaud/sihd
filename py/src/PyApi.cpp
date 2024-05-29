#include <algorithm>
#include <iostream>
#include <sihd/py/PyApi.hpp>

namespace sihd::py
{

std::map<std::string_view, std::function<void(PyApi::PyModule &)>> PyApi::api_lst;

PYBIND11_MODULE(sihd, m_sihd)
{
    m_sihd.doc() = "sihd";
    PyApi::set_api_to_module(m_sihd);
}

void PyApi::set_api_to_module(pybind11::module & m_sihd)
{
    PyModule pymodule(&m_sihd);

    for (const auto & pair : PyApi::api_lst)
    {
        pymodule.load(pair.first);
    }
}

void PyApi::add_api(std::string_view name, std::function<void(PyApi::PyModule &)> f)
{
    PyApi::api_lst[name] = f;
}

PyApi::PyModule::PyModule(pybind11::module *module): _module_ptr(module) {}

PyApi::PyModule::~PyModule() {}

bool PyApi::PyModule::is_loaded(std::string_view submodule_name)
{
    return std::find(_modules_imported.begin(), _modules_imported.end(), submodule_name) != _modules_imported.end();
}

bool PyApi::PyModule::load(std::string_view submodule_name)
{
    if (this->is_loaded(submodule_name))
        return true;
    auto it = PyApi::api_lst.find(submodule_name);
    if (it == PyApi::api_lst.end())
        return false;
    it->second(*this);
    _modules_imported.push_back(submodule_name);
    return true;
}

} // namespace sihd::py