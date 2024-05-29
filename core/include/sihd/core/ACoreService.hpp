#ifndef __SIHD_CORE_ACORESERVICE_HPP__
#define __SIHD_CORE_ACORESERVICE_HPP__

#include <sihd/util/AService.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/ServiceController.hpp>

namespace sihd::core
{

class ACoreService: public sihd::util::AService,
                    public sihd::util::IHandler<sihd::util::ServiceController *>
{
    public:
        using sihd::util::AService::init;
        using sihd::util::AService::is_running;
        using sihd::util::AService::reset;
        using sihd::util::AService::setup;
        using sihd::util::AService::start;
        using sihd::util::AService::stop;

        virtual ~ACoreService() {};

    protected:
        virtual void handle([[maybe_unused]] sihd::util::ServiceController *ctrl) override {}
};

} // namespace sihd::core

#endif