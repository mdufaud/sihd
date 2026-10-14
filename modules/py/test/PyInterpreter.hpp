#ifndef __SIHD_PY_PYINTERPRETER_HPP__
#define __SIHD_PY_PYINTERPRETER_HPP__

#include <pybind11/embed.h>

#include <memory>

#include <gtest/gtest.h>

namespace test
{

// one interpreter for the whole binary: pybind11 does not support finalizing
// then re-initializing (internals are freed, next get_internals is use-after-free).
// Owned by a gtest environment so the finalization runs inside main, before the
// leak check.
class __attribute__((visibility("hidden"))) PyEnvironment: public ::testing::Environment
{
    public:
        void SetUp() override
        {
            _interpreter = std::make_unique<pybind11::scoped_interpreter>();
            _current = _interpreter.get();
        }

        void TearDown() override
        {
            _current = nullptr;
            _interpreter.reset();
        }

        inline static pybind11::scoped_interpreter *_current = nullptr;

    private:
        std::unique_ptr<pybind11::scoped_interpreter> _interpreter;
};

inline const int py_env_registered = [] {
    ::testing::AddGlobalTestEnvironment(new PyEnvironment());
    return 0;
}();

inline pybind11::scoped_interpreter & py_interpreter()
{
    (void)py_env_registered;
    return *PyEnvironment::_current;
}

} // namespace test

#endif
