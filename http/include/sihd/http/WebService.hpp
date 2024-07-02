#ifndef __SIHD_HTTP_WEBSERVICE_HPP__
#define __SIHD_HTTP_WEBSERVICE_HPP__

#include <sihd/util/Callback.hpp>
#include <sihd/util/Node.hpp>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>

namespace sihd::http
{

class WebService: public sihd::util::Named
{
    public:
        WebService(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~WebService();

        virtual bool call(const std::string & path, const HttpRequest & request, HttpResponse & response);
        void set_entry_point(const std::string & path,
                             std::function<void(const HttpRequest &, HttpResponse &)> fun,
                             HttpRequest::RequestType type = HttpRequest::Get);
        template <class C>
        void set_entry_point(const std::string & path,
                             void (C::*method)(const HttpRequest &, HttpResponse &),
                             HttpRequest::RequestType type = HttpRequest::Get)
        {
            _callback_manager_map[type].set<C, void, const HttpRequest &, HttpResponse &>(path,
                                                                                          dynamic_cast<C *>(this),
                                                                                          method);
        }

    protected:

    private:
        std::map<HttpRequest::RequestType, sihd::util::CallbackManager> _callback_manager_map;
};

} // namespace sihd::http

#endif