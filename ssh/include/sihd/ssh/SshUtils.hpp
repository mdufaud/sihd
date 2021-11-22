#ifndef __SIHD_SSH_SSHUTILS_HPP__
# define __SIHD_SSH_SSHUTILS_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::ssh
{

class SshUtils:   public sihd::util::Named
{
    public:
        SshUtils(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~SshUtils();

    protected:

    private:
};

}

#endif