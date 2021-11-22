#ifndef __SIHD_SSH_SFTPCLIENT_HPP__
# define __SIHD_SSH_SFTPCLIENT_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::ssh
{

class SftpClient:   public sihd::util::Named
{
    public:
        SftpClient(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~SftpClient();

    protected:

    private:
};

}

#endif