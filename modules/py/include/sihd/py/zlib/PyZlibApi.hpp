#ifndef __SIHD_PY_PYZLIBAPI_HPP__
#define __SIHD_PY_PYZLIBAPI_HPP__

#include <sihd/py/PyApi.hpp>

namespace sihd::py
{

class PyZlibApi
{
    public:
        static void add_zlib_api(PyApi::PyModule & pymodule);

    private:
        ~PyZlibApi() = default;
};

} // namespace sihd::py

#endif
