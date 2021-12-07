#ifndef __SIHD_HTTP_HTTPSERVER_HPP__
# define __SIHD_HTTP_HTTPSERVER_HPP__

# include <sihd/util/Node.hpp>
# include <uWebSockets/App.h>

namespace sihd::http
{

class HttpServer:   public sihd::util::Named
{
    public:
        HttpServer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~HttpServer();

    protected:

    private:
};

}

#endif