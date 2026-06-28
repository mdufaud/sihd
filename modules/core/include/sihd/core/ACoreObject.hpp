#ifndef __SIHD_CORE_ACOREOBJECT_HPP__
#define __SIHD_CORE_ACOREOBJECT_HPP__

#include <sihd/util/Configurable.hpp>
#include <sihd/util/Node.hpp>

#include <sihd/core/ACoreService.hpp>
#include <sihd/core/Channel.hpp>

namespace sihd::core
{

class ACoreObject: public sihd::util::Named,
                   public sihd::util::Configurable,
                   public sihd::util::IHandler<Channel *>,
                   public ACoreService
{
    public:
        using ACoreService::init;
        using ACoreService::is_running;
        using ACoreService::reset;
        using ACoreService::setup;
        using ACoreService::start;
        using ACoreService::stop;

        ACoreObject(const std::string & name, sihd::util::Node *parent = nullptr):
            sihd::util::Named(name, parent)
        {
        }
        virtual ~ACoreObject() = default;

        bool is_running() const override { return false; }

    protected:
        using ACoreService::handle;

        virtual void handle([[maybe_unused]] Channel *channel) override {};
};

} // namespace sihd::core

#endif