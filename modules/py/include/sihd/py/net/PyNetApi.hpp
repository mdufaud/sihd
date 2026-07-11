#ifndef __SIHD_PY_PYNETAPI_HPP__
#define __SIHD_PY_PYNETAPI_HPP__

#include <sihd/py/PyApi.hpp>

namespace sihd::py
{

class PyNetApi
{
    public:
        static void add_net_api(PyApi::PyModule & pymodule);

    private:
        ~PyNetApi() = default;
};

} // namespace sihd::py

#endif
