#ifndef __SIHD_NET_PINGER_HPP__
# define __SIHD_NET_PINGER_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::net
{

class Pinger:   public sihd::util::Named
{
    public:
        Pinger(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~Pinger();

    protected:

    private:
};

}

#endif