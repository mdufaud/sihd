#ifndef __SIHD_PY_PYAPI_HPP__
#define __SIHD_PY_PYAPI_HPP__

// pybind11 (Python.h) must precede std headers: macro redefinition with Python 3.14+ and glibc 2.42+
#include <pybind11/pybind11.h>

#include <functional>
#include <map>
#include <string_view>
#include <vector>

namespace sihd::py
{

class PyApi
{
    public:
        class __attribute__((visibility("hidden"))) PyModule
        {
            public:
                PyModule(pybind11::module *module);
                ~PyModule();

                bool load(std::string_view submodule_name);
                bool is_loaded(std::string_view submodule_name);

                pybind11::module & module() const;

            protected:

            private:
                pybind11::module *_module_ptr;
                std::vector<std::string_view> _modules_imported;
        };

        static void set_api_to_module(pybind11::module &);

        static void add_api(std::string_view name, std::function<void(PyModule &)> f);

    protected:
        static std::map<std::string_view, std::function<void(PyModule &)>> & api_lst();

    private:
        virtual ~PyApi() = default;
        ;
};

} // namespace sihd::py

#endif