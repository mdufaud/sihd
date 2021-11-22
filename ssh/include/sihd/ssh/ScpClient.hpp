#ifndef __SIHD_SSH_SCPCLIENT_HPP__
# define __SIHD_SSH_SCPCLIENT_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::ssh
{

class ScpClient:   public sihd::util::Named
{
    public:
        ScpClient(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~ScpClient();

    protected:

    private:
};

}

#endif