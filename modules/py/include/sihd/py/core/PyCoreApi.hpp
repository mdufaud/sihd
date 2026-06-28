#ifndef __SIHD_PY_PYCOREAPI_HPP__
#define __SIHD_PY_PYCOREAPI_HPP__

#include <sihd/py/PyApi.hpp>

#include <sihd/core/Channel.hpp>
#include <sihd/core/Device.hpp>

namespace sihd::py
{

class PyCoreApi
{
    public:
        static void add_core_api(PyApi::PyModule & pymodule);

        class __attribute__((visibility("hidden"))) PyChannelHandler
            : public sihd::util::IHandler<sihd::core::Channel *>
        {
            public:
                PyChannelHandler();
                ~PyChannelHandler();

                void handle(sihd::core::Channel *c) override;

                void add_channel_obs(sihd::core::Channel *c, pybind11::function & fun);
                void remove_channel_obs(sihd::core::Channel *c);

            protected:

            private:
        };

    protected:
        // important because notify might have a python function
        template <typename T>
        static bool _unlock_gil_write_channel(sihd::core::Channel *self, size_t idx, T val)
        {
            pybind11::gil_scoped_release release;
            return self->write<T>(idx, val);
        }

    private:
        ~PyCoreApi() = default;
        ;

        static PyChannelHandler _channel_handler;
};

} // namespace sihd::py

#endif