#ifndef __SIHD_CORE_CORE_HPP__
#define __SIHD_CORE_CORE_HPP__

#include <sihd/core/Device.hpp>

namespace sihd::core
{

class Core: public sihd::core::Device
{
    public:
        Core(const std::string & name = "core", sihd::util::Node *parent = nullptr);
        virtual ~Core();

        bool is_running() const override;

        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

    private:
        bool _running;
        bool _is_reset;
};

} // namespace sihd::core

#endif