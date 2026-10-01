#ifndef __SIHD_PY_EXPECTED_CASTER_HPP__
#define __SIHD_PY_EXPECTED_CASTER_HPP__

#include <pybind11/pybind11.h>

#include <expected>
#include <stdexcept>

#include <sihd/util/Error.hpp>

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
    pybind11::object instance = error_type()(error.message);
    instance.attr("code") = static_cast<int>(error.code);
    PyErr_SetObject(error_type().ptr(), instance.ptr());
    throw pybind11::error_already_set();
}

} // namespace sihd::py

namespace pybind11::detail
{

// cast a returned std::expected to its value; an error raises sihd.Error
template <typename T>
struct type_caster<std::expected<T, sihd::util::Error>>
{
    public:
        static constexpr auto name = const_name("sihd.Expected[") + make_caster<T>::name + const_name("]");

        static handle cast(const std::expected<T, sihd::util::Error> & src, return_value_policy policy, handle parent)
        {
            if (!src)
                sihd::py::raise_error(src.error());
            return make_caster<T>::cast(*src, policy, parent);
        }
};

template <>
struct type_caster<std::expected<void, sihd::util::Error>>
{
    public:
        static constexpr auto name = const_name("None");

        static handle cast(const std::expected<void, sihd::util::Error> & src, return_value_policy, handle)
        {
            if (!src)
                sihd::py::raise_error(src.error());
            return pybind11::none().release();
        }
};

} // namespace pybind11::detail

#endif
