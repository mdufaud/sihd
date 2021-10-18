#ifndef __SIHD_CORE_ACORESERVICE_HPP__
# define __SIHD_CORE_ACORESERVICE_HPP__

# include <sihd/util/AService.hpp>
# include <sihd/util/ServiceController.hpp>
# include <sihd/util/IObserver.hpp>

namespace sihd::core
{

class ACoreService: virtual public sihd::util::AService,
                    virtual public sihd::util::IObserver<sihd::util::ServiceController>
{
    public:
        virtual ~ACoreService() {};

        virtual void observable_changed([[maybe_unused]] sihd::util::ServiceController *ctrl) override {}
};

}

#endif