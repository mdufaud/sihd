#ifndef __SIHD_PY_PYUTILAPI_HPP__
#define __SIHD_PY_PYUTILAPI_HPP__

#include <pybind11/chrono.h>
#include <pybind11/stl.h>

#include <sihd/py/PyApi.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>
#include <sihd/util/Task.hpp>

PYBIND11_DECLARE_HOLDER_TYPE(T, sihd::util::SmartNodePtr<T>);

namespace sihd::py
{

class PyUtilApi
{
    public:
        static void add_util_api(PyApi::PyModule & pymodule);

        class __attribute__((visibility("hidden"))) PyTask: public sihd::util::Task
        {
            public:
                PyTask(const pybind11::function & task, const sihd::util::TaskOptions & options);
                virtual ~PyTask();

                virtual bool run();

            private:
                pybind11::function _fun;
        };

    protected:
        static bool _configurable_set_conf(sihd::util::Configurable *self, const pybind11::kwargs & kwargs);
        static bool _configurable_set_single_conf(sihd::util::Configurable *self,
                                                  const std::string & key,
                                                  const pybind11::handle & handle);

    private:
        ~PyUtilApi() = default;
        ;
};

} // namespace sihd::py

#endif
