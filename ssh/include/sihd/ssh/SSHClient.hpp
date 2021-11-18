#ifndef __SIHD_SSH_SSHCLIENT_HPP__
# define __SIHD_SSH_SSHCLIENT_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::ssh
{

class SSHClient:   public sihd::util::Named
{
    public:
        SSHClient(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~SSHClient();

    protected:

    private:
};

}

#endif