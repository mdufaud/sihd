#ifndef __SIHD_CORE_CORE_HPP__
# define __SIHD_CORE_CORE_HPP__

# include <sihd/core/Device.hpp>

namespace sihd::core
{

// Only purpose is to be a container and cascade service states to children

class Core:   public sihd::core::Device
{
    public:
        Core(const std::string & name = "core", sihd::util::Node *parent = nullptr);
        virtual ~Core();

        bool is_running() const { return false; }
};

}

#endif