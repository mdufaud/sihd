#include <pybind11/functional.h>
#include <pybind11/stl.h>

#include <chrono>
#include <functional>

#include <sihd/py/http/PyHttpApi.hpp>

#include <sihd/util/Configurable.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>

#include <sihd/http/CurlOptions.hpp>
#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/HttpServer.hpp>
#include <sihd/http/Navigator.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/http/navigator/NavigatorResponse.hpp>
#include <sihd/http/request.hpp>

namespace sihd::py
{

using namespace sihd::http;
using sihd::util::Configurable;
using sihd::util::Node;
using sihd::util::SmartNodePtr;

SIHD_LOGGER;

namespace
{

pybind11::dict headers_to_dict(const HttpResponse & resp)
{
    pybind11::dict headers;
    for (const auto & [name, value] : resp.http_header().headers())
        headers[pybind11::str(name)] = value;
    return headers;
}

} // namespace

void PyHttpApi::add_http_api(PyApi::PyModule & pymodule)
{
    auto m_sihd = pymodule.module();
    if (pymodule.load("sihd_core") == false)
        return;

    pybind11::module m_http = m_sihd.def_submodule("http", "sihd::http");

    pybind11::enum_<HttpRequest::RequestType>(m_http, "RequestType")
        .value("NoType", HttpRequest::None)
        .value("Post", HttpRequest::Post)
        .value("Get", HttpRequest::Get)
        .value("Put", HttpRequest::Put)
        .value("Delete", HttpRequest::Delete)
        .value("Options", HttpRequest::Options)
        .value("Patch", HttpRequest::Patch)
        .value("Head", HttpRequest::Head);

    auto optional_sv = [](std::optional<std::string_view> v) -> pybind11::object {
        if (v.has_value())
            return pybind11::str(std::string(*v));
        return pybind11::none();
    };

    pybind11::class_<HttpRequest>(m_http, "HttpRequest")
        .def("type", &HttpRequest::request_type)
        .def("type_str", static_cast<std::string (HttpRequest::*)() const>(&HttpRequest::type_str))
        .def("url", &HttpRequest::url)
        .def("client_ip", &HttpRequest::client_ip)
        .def("has_content", &HttpRequest::has_content)
        .def("content", [](const HttpRequest & self) { return pybind11::bytes(self.content().cpp_str()); })
        .def("text", [](const HttpRequest & self) { return self.content().cpp_str(); })
        .def("path_params", &HttpRequest::path_params)
        .def("query_params", &HttpRequest::query_params)
        .def("cookies", &HttpRequest::cookies)
        .def("path_param", [optional_sv](const HttpRequest & self, const std::string & n) { return optional_sv(self.path_param(n)); })
        .def("query_param", [optional_sv](const HttpRequest & self, const std::string & n) { return optional_sv(self.query_param(n)); })
        .def("cookie", [optional_sv](const HttpRequest & self, const std::string & n) { return optional_sv(self.cookie(n)); })
        .def("is_authenticated", &HttpRequest::is_authenticated)
        .def("auth_user", &HttpRequest::auth_user)
        .def("auth_token", &HttpRequest::auth_token);

    pybind11::class_<HttpResponse>(m_http, "HttpResponse")
        .def("status", &HttpResponse::status)
        .def("content", [](const HttpResponse & self) { return pybind11::bytes(self.content().cpp_str()); })
        .def("text", [](const HttpResponse & self) { return self.content().cpp_str(); })
        .def("headers", &headers_to_dict)
        .def("is_streaming", &HttpResponse::is_streaming)
        // handler-side setters
        .def("set_status", &HttpResponse::set_status)
        .def("set_plain_content", &HttpResponse::set_plain_content)
        .def("set_content",
             [](HttpResponse & self, std::string_view data) { return self.set_content(data); })
        .def("set_byte_content",
             [](HttpResponse & self, pybind11::bytes data) {
                 std::string bytes = data;
                 return self.set_byte_content(sihd::util::ArrByteView(bytes.data(), bytes.size()));
             })
        .def("set_content_type", &HttpResponse::set_content_type)
        .def("set_content_type_from_extension", &HttpResponse::set_content_type_from_extension)
        .def("set_cookie",
             &HttpResponse::set_cookie,
             pybind11::arg("name"),
             pybind11::arg("value"),
             pybind11::arg("options") = std::string_view {});

    pybind11::class_<NavigatorResponse, HttpResponse>(m_http, "NavigatorResponse")
        .def("cookies", &NavigatorResponse::cookies)
        .def("redirect_history", &NavigatorResponse::redirect_history)
        .def("final_url", &NavigatorResponse::final_url)
        .def("was_method_downgraded", &NavigatorResponse::was_method_downgraded);

    pybind11::class_<CurlFileOptions>(m_http, "CurlFileOptions")
        .def(pybind11::init<>())
        .def_readwrite("form_name", &CurlFileOptions::form_name)
        .def_readwrite("file_path", &CurlFileOptions::file_path)
        .def_readwrite("file_name", &CurlFileOptions::file_name)
        .def_readwrite("data", &CurlFileOptions::data);

    pybind11::class_<CurlOptions>(m_http, "CurlOptions")
        .def(pybind11::init<>())
        .def_readwrite("verbose", &CurlOptions::verbose)
        .def_readwrite("follow_location", &CurlOptions::follow_location)
        .def_readwrite("timeout_s", &CurlOptions::timeout_s)
        .def_readwrite("connect_timeout_s", &CurlOptions::connect_timeout_s)
        .def_readwrite("ssl_verify_peer", &CurlOptions::ssl_verify_peer)
        .def_readwrite("ssl_verify_host", &CurlOptions::ssl_verify_host)
        .def_readwrite("parameters", &CurlOptions::parameters)
        .def_readwrite("headers", &CurlOptions::headers)
        .def_readwrite("username", &CurlOptions::username)
        .def_readwrite("password", &CurlOptions::password)
        .def_readwrite("token", &CurlOptions::token)
        .def_readwrite("user_agent", &CurlOptions::user_agent)
        .def_readwrite("proxy", &CurlOptions::proxy)
        .def_readwrite("proxy_username", &CurlOptions::proxy_username)
        .def_readwrite("proxy_password", &CurlOptions::proxy_password)
        .def_readwrite("file", &CurlOptions::file)
        .def_readwrite("form_parameters", &CurlOptions::form_parameters)
        .def_readwrite("upload_progress", &CurlOptions::upload_progress);

    pybind11::enum_<ProxyType>(m_http, "ProxyType")
        .value("Http", ProxyType::Http)
        .value("Socks4", ProxyType::Socks4)
        .value("Socks5", ProxyType::Socks5);

    pybind11::class_<Navigator>(m_http, "Navigator")
        .def(pybind11::init<>())
        // HTTP methods (release the GIL: blocking I/O, and lets an in-process
        // python route handler acquire the GIL while a request is in flight)
        .def("get", &Navigator::get, pybind11::arg("url"), pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "post",
            [](Navigator & self, std::string_view url, std::string_view data) { return self.post(url, data); },
            pybind11::arg("url"),
            pybind11::arg("data") = std::string_view {},
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("put",
             &Navigator::put,
             pybind11::arg("url"),
             pybind11::arg("data"),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("patch",
             &Navigator::patch,
             pybind11::arg("url"),
             pybind11::arg("data"),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("delete", &Navigator::del, pybind11::arg("url"), pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("head", &Navigator::head, pybind11::arg("url"), pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("options",
             &Navigator::options,
             pybind11::arg("url"),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("download",
             &Navigator::download,
             pybind11::arg("url"),
             pybind11::arg("path"),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        // configuration
        .def("set_verbose", &Navigator::set_verbose)
        .def("set_follow_redirects", &Navigator::set_follow_redirects)
        .def("set_max_redirects", &Navigator::set_max_redirects)
        .def("set_timeout", &Navigator::set_timeout)
        .def("set_connect_timeout", &Navigator::set_connect_timeout)
        .def("set_accept_encoding", &Navigator::set_accept_encoding)
        .def("set_http2", &Navigator::set_http2)
        .def("set_ssl_verify", &Navigator::set_ssl_verify)
        .def("set_user_agent", &Navigator::set_user_agent)
        .def("set_max_response_size", &Navigator::set_max_response_size)
        .def("set_ssrf_guard", &Navigator::set_ssrf_guard)
        // authentication
        .def("set_basic_auth", &Navigator::set_basic_auth)
        .def("set_digest_auth", &Navigator::set_digest_auth)
        .def("set_token_auth", &Navigator::set_token_auth)
        .def("clear_auth", &Navigator::clear_auth)
        // persistent headers
        .def("set_header", &Navigator::set_header)
        .def("remove_header", &Navigator::remove_header)
        .def("clear_headers", &Navigator::clear_headers)
        // cookies
        .def("cookies", &Navigator::cookies)
        .def("set_cookie",
             &Navigator::set_cookie,
             pybind11::arg("name"),
             pybind11::arg("value"),
             pybind11::arg("domain") = std::string_view {})
        .def("clear_cookies", &Navigator::clear_cookies)
        .def("save_cookies", &Navigator::save_cookies)
        .def("load_cookies", &Navigator::load_cookies)
        // proxy
        .def("set_proxy", &Navigator::set_proxy, pybind11::arg("url"), pybind11::arg("type") = ProxyType::Http)
        .def("set_proxy_auth", &Navigator::set_proxy_auth)
        .def("clear_proxy", &Navigator::clear_proxy);

    // stateless request helpers (CurlOptions consumers)
    m_http.def(
        "get",
        [](std::string_view url, const CurlOptions & opt) { return get(url, opt); },
        pybind11::arg("url"),
        pybind11::arg("options") = CurlOptions::none(),
        pybind11::call_guard<pybind11::gil_scoped_release>());
    m_http.def(
        "post",
        [](std::string_view url, std::string_view data, const CurlOptions & opt) { return post(url, data, opt); },
        pybind11::arg("url"),
        pybind11::arg("data") = std::string_view {},
        pybind11::arg("options") = CurlOptions::none(),
        pybind11::call_guard<pybind11::gil_scoped_release>());
    m_http.def(
        "put",
        [](std::string_view url, std::string_view file_path, const CurlOptions & opt) { return put(url, file_path, opt); },
        pybind11::arg("url"),
        pybind11::arg("file_path"),
        pybind11::arg("options") = CurlOptions::none(),
        pybind11::call_guard<pybind11::gil_scoped_release>());
    m_http.def(
        "delete",
        [](std::string_view url, const CurlOptions & opt) { return del(url, opt); },
        pybind11::arg("url"),
        pybind11::arg("options") = CurlOptions::none(),
        pybind11::call_guard<pybind11::gil_scoped_release>());
    m_http.def(
        "head",
        [](std::string_view url, const CurlOptions & opt) { return head(url, opt); },
        pybind11::arg("url"),
        pybind11::arg("options") = CurlOptions::none(),
        pybind11::call_guard<pybind11::gil_scoped_release>());

    // --- server side ---

    pybind11::class_<WebService, sihd::util::Named, SmartNodePtr<WebService>>(m_http, "WebService")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def(
            "set_entry_point",
            // The handler runs on a server worker thread and the route table COPIES
            // the std::function on every dispatch. Hold the python callable through a
            // shared_ptr so those copies never touch its refcount without the GIL; the
            // deleter re-takes the GIL so the final release is also safe. At call time
            // acquire the GIL and pass request/response as non-owning references (they
            // are move-only C++ objects owned by the server; a copy policy would fail).
            [](WebService & self, const std::string & path, pybind11::function fun, HttpRequest::RequestType type) {
                auto fun_ptr = std::shared_ptr<pybind11::function>(new pybind11::function(std::move(fun)),
                                                                   [](pybind11::function *p) {
                                                                       pybind11::gil_scoped_acquire acquire;
                                                                       delete p;
                                                                   });
                self.set_entry_point(
                    path,
                    [fun_ptr](const HttpRequest & req, HttpResponse & resp) {
                        pybind11::gil_scoped_acquire acquire;
                        (*fun_ptr)(pybind11::cast(&req, pybind11::return_value_policy::reference),
                                   pybind11::cast(&resp, pybind11::return_value_policy::reference));
                    },
                    type);
            },
            pybind11::arg("path"),
            pybind11::arg("handler"),
            pybind11::arg("type") = HttpRequest::Get);

    pybind11::class_<HttpServer, Node, Configurable, SmartNodePtr<HttpServer>>(m_http, "HttpServer")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def("set_port", &HttpServer::set_port)
        .def("set_root_dir", &HttpServer::set_root_dir)
        .def("set_server_name", &HttpServer::set_server_name)
        .def("set_cors_origin", &HttpServer::set_cors_origin)
        .def("set_404_path", &HttpServer::set_404_path)
        .def(
            "add_web_service",
            [](HttpServer & self, const std::string & name) { return self.add_child<WebService>(name); },
            pybind11::arg("name"),
            pybind11::return_value_policy::reference_internal)
        .def("start", [](HttpServer & self) { return self.start(); }, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("stop", [](HttpServer & self) { return self.stop(); }, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("request_stop", &HttpServer::request_stop)
        .def("set_service_wait_stop", &HttpServer::set_service_wait_stop)
        .def("is_running", [](const HttpServer & self) { return self.is_running(); })
        .def(
            "wait_ready",
            [](const HttpServer & self, int ms) { return self.wait_ready(std::chrono::milliseconds(ms)); },
            pybind11::arg("timeout_ms"),
            pybind11::call_guard<pybind11::gil_scoped_release>());
}

static void __attribute__((constructor)) premain()
{
    PyApi::add_api("sihd_http", &PyHttpApi::add_http_api);
}

} // namespace sihd::py
