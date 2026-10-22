#ifndef __SIHD_PY_PYINTERPRETER_HPP__
#define __SIHD_PY_PYINTERPRETER_HPP__

#include <pybind11/embed.h>

#include <memory>

#include <gtest/gtest.h>

namespace test
{

// one interpreter per binary: pybind11 cannot re-initialize after finalize
class __attribute__((visibility("hidden"))) PyEnvironment: public ::testing::Environment
{
    public:
        void SetUp() override { _interpreter = std::make_unique<pybind11::scoped_interpreter>(); }

        void TearDown() override { _interpreter.reset(); }

    private:
        std::unique_ptr<pybind11::scoped_interpreter> _interpreter;
};

inline const int py_env_registered = [] {
    ::testing::AddGlobalTestEnvironment(new PyEnvironment());
    return 0;
}();

inline void require_py_environment()
{
    (void)py_env_registered;
}

} // namespace test

#endif
