#ifndef __SIHD_PY_PYWIFIAPI_HPP__
#define __SIHD_PY_PYWIFIAPI_HPP__

#include <sihd/py/PyApi.hpp>

namespace sihd::py
{

class PyWifiApi
{
    public:
        static void add_wifi_api(PyApi::PyModule & pymodule);

    private:
        ~PyWifiApi() = default;
};

} // namespace sihd::py

#endif
