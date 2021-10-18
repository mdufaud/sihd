#ifndef __SIHD_CORE_ACOREOBJECT_HPP__
# define __SIHD_CORE_ACOREOBJECT_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/core/ACoreService.hpp>

namespace sihd::core
{

class ACoreObject:  public sihd::util::Named,
                    public sihd::util::Configurable,
                    public sihd::util::IObserver<Channel>,
                    public ACoreService
{
    public:
        ACoreObject(const std::string & name, sihd::util::Node *parent = nullptr): sihd::util::Named(name, parent) {}
        virtual ~ACoreObject() {}

        bool is_running() const { return false; }

    protected:
        void observable_changed([[maybe_unused]] Channel *channel) {};
};

}

#endif