#ifndef __SIHD_HTTP_WEBSERVICE_HPP__
#define __SIHD_HTTP_WEBSERVICE_HPP__

#include <sihd/util/Node.hpp>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/IHttpAuthenticator.hpp>
#include <sihd/http/Route.hpp>

namespace sihd::http
{

class WebService: public sihd::util::Named
{
    public:
        WebService(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~WebService();

        virtual bool call(std::string_view path, HttpRequest & request, HttpResponse & response);

        void set_entry_point(const std::string & path,
                             std::function<void(const HttpRequest &, HttpResponse &)> fun,
                             HttpRequest::RequestType type = HttpRequest::Get);

        void set_authenticator(IHttpAuthenticator *authenticator) { _authenticator = authenticator; }
        IHttpAuthenticator *authenticator() const { return _authenticator; }

        template <class C>
        void set_entry_point(const std::string & path,
                             void (C::*method)(const HttpRequest &, HttpResponse &),
                             HttpRequest::RequestType type = HttpRequest::Get)
        {
            C *self = dynamic_cast<C *>(this);
            _route_table.add(
                path,
                [self, method](const HttpRequest & req, HttpResponse & resp) { (self->*method)(req, resp); },
                type);
        }

    private:
        IHttpAuthenticator *_authenticator = nullptr;
        RouteTable _route_table;
};

} // namespace sihd::http

#endif