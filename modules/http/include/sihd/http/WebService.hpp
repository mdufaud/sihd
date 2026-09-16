#ifndef __SIHD_HTTP_WEBSERVICE_HPP__
#define __SIHD_HTTP_WEBSERVICE_HPP__

#include <functional>

#include <sihd/http/BodyStream.hpp>
#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/IHttpAuthenticator.hpp>
#include <sihd/http/Route.hpp>
#include <sihd/util/Node.hpp>

namespace sihd::http
{

class WebService: public sihd::util::Named
{
    public:
        WebService(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~WebService();

        virtual bool call(std::string_view path, HttpRequest & request, HttpResponse & response);

        // methods the webservice answers for the path, empty when it knows no such path
        std::vector<HttpRequest::RequestType> allowed_methods(std::string_view path) const;

        void set_entry_point(const std::string & path,
                             std::function<void(const HttpRequest &, HttpResponse &)> fun,
                             HttpRequest::RequestType type = HttpRequest::Get);

        void set_authenticator(IHttpAuthenticator *authenticator) { _authenticator = authenticator; }
        IHttpAuthenticator *authenticator() const { return _authenticator; }

        // without a stream the body is buffered in the request
        void set_body_stream(IBodyStream *stream);
        IBodyStream *body_stream() const;

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
        IBodyStream *_body_stream_ptr = nullptr;
        RouteTable _route_table;
};

} // namespace sihd::http

#endif