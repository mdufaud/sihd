#ifndef __SIHD_PY_PYSYSAPI_HPP__
#define __SIHD_PY_PYSYSAPI_HPP__

#include <sihd/py/PyApi.hpp>

namespace sihd::py
{

class PySysApi
{
    public:
        static void add_sys_api(PyApi::PyModule & pymodule);

    private:
        ~PySysApi() = default;
};

} // namespace sihd::py

#endif
