#ifndef __SIHD_PY_PYBTAPI_HPP__
#define __SIHD_PY_PYBTAPI_HPP__

#include <sihd/py/PyApi.hpp>

namespace sihd::py
{

class PyBtApi
{
    public:
        static void add_bt_api(PyApi::PyModule & pymodule);

    private:
        ~PyBtApi() = default;
};

} // namespace sihd::py

#endif
