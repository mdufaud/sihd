#ifndef __SIHD_PY_PYCOREAPI_HPP__
# define __SIHD_PY_PYCOREAPI_HPP__

# include <sihd/py/PyApi.hpp>
# include <sihd/core/Device.hpp>

namespace sihd::py
{

class PyCoreApi
{
    public:
        static void add_core_api(PyApi::PyModule & pymodule);

        class __attribute__ ((visibility("hidden"))) PyChannelHandler: public sihd::util::IHandler<sihd::core::Channel *>
        {
            public:
                PyChannelHandler();
                ~PyChannelHandler();

                void handle(sihd::core::Channel *c) override;

                void add_channel_obs(sihd::core::Channel *c, pybind11::function & fun);
                void remove_channel_obs(sihd::core::Channel *c);

            protected:
                bool handle_channel_thread();

            private:
                std::map<sihd::core::Channel *, pybind11::function> _map_handler;
                std::mutex _mutex_map;
        };

    protected:

    private:
        ~PyCoreApi() {};

        static PyChannelHandler _channel_handler;
};

}

#endif