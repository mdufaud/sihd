#ifndef __SIHD_PY_PYUTILAPI_HPP__
# define __SIHD_PY_PYUTILAPI_HPP__

# include <pybind11/pybind11.h>
# include <sihd/util/SmartNodePtr.hpp>

PYBIND11_DECLARE_HOLDER_TYPE(T, sihd::util::SmartNodePtr<T>);

namespace sihd::py
{

class PyUtilApi
{
    public:

    private:
        PyUtilApi() {};
        ~PyUtilApi() {};
};

}

#endif
