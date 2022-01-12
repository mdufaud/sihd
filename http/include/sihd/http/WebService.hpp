#ifndef __SIHD_HTTP_WEBSERVICE_HPP__
# define __SIHD_HTTP_WEBSERVICE_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::http
{

class WebService:   public sihd::util::Named
{
    public:
        WebService(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~WebService();

    protected:

    private:
};

}

#endif