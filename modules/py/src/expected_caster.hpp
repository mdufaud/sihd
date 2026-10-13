#ifndef __SIHD_PY_EXPECTED_CASTER_HPP__
#define __SIHD_PY_EXPECTED_CASTER_HPP__

#include <pybind11/pybind11.h>

#include <stdexcept>

#include <sihd/util/Result.hpp>

namespace sihd::py
{

// the sihd.Error python exception (RuntimeError base): args = (message,), code attribute = ErrorCode
inline pybind11::object & error_type()
{
    static pybind11::object type = pybind11::reinterpret_steal<pybind11::object>(
        PyErr_NewException("sihd.Error", PyExc_RuntimeError, nullptr));
    return type;
}

[[noreturn]] inline void raise_error(const sihd::util::Error & error)
{
    // callers may release the GIL for the I/O (call_guard): raising touches the interpreter
    pybind11::gil_scoped_acquire acquire;
    // fetch the type from the module, not from a C++ static: inline statics are not
    // deduplicated across the shared libraries that instantiate this header
    pybind11::object type = pybind11::module_::import("sihd").attr("Error");
    pybind11::object instance = type(error.message);
    instance.attr("code") = static_cast<int>(error.code);
    PyErr_SetObject(type.ptr(), instance.ptr());
    throw pybind11::error_already_set();
}

} // namespace sihd::py

namespace pybind11::detail
{

// cast a returned sihd::util::Result to its value; an error raises sihd.Error
template <typename T>
struct type_caster<sihd::util::Result<T>>
{
    public:
        static constexpr auto name = const_name("sihd.Result[") + make_caster<T>::name + const_name("]");

        static handle cast(sihd::util::Result<T> && src, return_value_policy policy, handle parent)
        {
            if (!src)
                sihd::py::raise_error(src.error());
            return make_caster<T>::cast(std::move(*src), policy, parent);
        }

        static handle cast(const sihd::util::Result<T> & src, return_value_policy policy, handle parent)
        {
            if (!src)
                sihd::py::raise_error(src.error());
            return make_caster<T>::cast(*src, policy, parent);
        }
};

template <>
struct type_caster<sihd::util::Result<void>>
{
    public:
        static constexpr auto name = const_name("None");

        static handle cast(const sihd::util::Result<void> & src, return_value_policy, handle)
        {
            if (!src)
                sihd::py::raise_error(src.error());
            return pybind11::none().release();
        }
};

} // namespace pybind11::detail

#endif
