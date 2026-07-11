#ifndef __SIHD_PY_PYHTTPAPI_HPP__
#define __SIHD_PY_PYHTTPAPI_HPP__

#include <sihd/py/PyApi.hpp>

namespace sihd::py
{

class PyHttpApi
{
    public:
        static void add_http_api(PyApi::PyModule & pymodule);

    private:
        ~PyHttpApi() = default;
};

} // namespace sihd::py

#endif
