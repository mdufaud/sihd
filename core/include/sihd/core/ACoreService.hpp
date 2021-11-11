#ifndef __SIHD_CORE_ACORESERVICE_HPP__
# define __SIHD_CORE_ACORESERVICE_HPP__

# include <sihd/util/AService.hpp>
# include <sihd/util/ServiceController.hpp>
# include <sihd/util/IHandler.hpp>

namespace sihd::core
{

class ACoreService: public sihd::util::AService,
                    public sihd::util::IHandler<sihd::util::ServiceController *>
{
    public:
        virtual ~ACoreService() {};

    protected:
        virtual void handle([[maybe_unused]] sihd::util::ServiceController *ctrl) override {}
};

}

#endif