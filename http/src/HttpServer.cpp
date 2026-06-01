#include <sihd/http/HttpServer.hpp>
#include <sihd/http/WriteProtocol.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

#include "server/HttpServerImpl.hpp"

namespace sihd::http
{

SIHD_REGISTER_FACTORY(HttpServer)

SIHD_NEW_LOGGER("sihd::http");

using namespace sihd::util;
using namespace sihd::sys;

int write_protocol_to_lws(WriteProtocol protocol);

HttpServer::HttpServer(const std::string & name, sihd::util::Node *parent):
    sihd::util::Node(name, parent),
    _impl(std::make_unique<Impl>(this))
{
    this->add_conf("encoding", &HttpServer::set_encoding);
    this->add_conf("port", &HttpServer::set_port);
    this->add_conf("root_dir", &HttpServer::set_root_dir);
    this->add_conf("poll_frequency", &HttpServer::set_poll_frequency);
    this->add_conf("ssl_cert_path", &HttpServer::set_ssl_cert_path);
    this->add_conf("ssl_cert_key", &HttpServer::set_ssl_cert_key);
    this->add_conf("404_path", &HttpServer::set_404_path);
    this->add_conf("server_name", &HttpServer::set_server_name);
    this->add_conf("cors_origin", &HttpServer::set_cors_origin);
    this->add_conf("resource_path", &HttpServer::add_resource_path);
    this->add_conf("service_thread_count", &HttpServer::set_service_thread_count);
}

HttpServer::~HttpServer()
{
    if (this->is_running())
        this->stop();
}

bool HttpServer::set_encoding(std::string_view encoding)
{
    _impl->encoding = encoding;
    return true;
}

bool HttpServer::set_port(int port)
{
    _impl->port = port;
    return true;
}

bool HttpServer::set_root_dir(std::string_view root_dir)
{
    bool ret = fs::is_dir(root_dir);
    if (ret)
        _impl->root_dir = fs::realpath(root_dir);
    else
        SIHD_LOG(error, "HttpServer: root dir does not exists: {}", root_dir);
    return ret;
}

bool HttpServer::set_poll_frequency(double freq)
{
    return _impl->stepworker.set_frequency(freq);
}

bool HttpServer::set_ssl_cert_path(std::string_view path)
{
    bool ret = fs::is_file(path);
    if (ret)
        _impl->ssl_cert_path = path;
    else
        SIHD_LOG(error, "HttpServer: ssl cert path does not exists: {}", path);
    return ret;
}

bool HttpServer::set_ssl_cert_key(std::string_view path)
{
    bool ret = fs::is_file(path);
    if (ret)
        _impl->ssl_cert_key = path;
    else
        SIHD_LOG(error, "HttpServer: ssl cert key does not exists: {}", path);
    return ret;
}

bool HttpServer::add_resource_path(const std::string & path)
{
    _impl->resources_path.insert(path);
    return true;
}

bool HttpServer::remove_resource_path(const std::string & path)
{
    auto it = _impl->resources_path.find(path);
    if (it != _impl->resources_path.end())
        _impl->resources_path.erase(it);
    return true;
}

bool HttpServer::set_404_path(std::string_view path)
{
    _impl->page_404_path = path;
    return true;
}

bool HttpServer::set_server_name(std::string_view name)
{
    _impl->default_server_name = name;
    return true;
}

bool HttpServer::set_cors_origin(std::string_view origin)
{
    _impl->default_cors_origin = origin;
    return true;
}

void HttpServer::set_http_filter(IHttpFilter *filter)
{
    _impl->http_filter = filter;
}

void HttpServer::set_authenticator(IHttpAuthenticator *authenticator)
{
    _impl->http_authenticator = authenticator;
}

bool HttpServer::set_service_thread_count(size_t count)
{
    _impl->service_thread_count = count < 1 ? 1 : count;
    return true;
}

void HttpServer::request_stop()
{
    _impl->stop = true;
}

bool HttpServer::on_stop()
{
    _impl->stop = true;
    {
        std::lock_guard l(_impl->mutex);
        if (_impl->lws_context_ptr != nullptr)
            lws_cancel_service(_impl->lws_context_ptr);
    }
    return true;
}

bool HttpServer::on_start()
{
    _impl->stop = false;

    struct lws_context_creation_info lws_info;
    memset(&lws_info, 0, sizeof(lws_context_creation_info));
    lws_info.port = _impl->port;
    lws_info.iface = nullptr;
    lws_info.protocols = _impl->lws_protocols_ptr;
    lws_info.gid = -1;
    lws_info.uid = -1;
    lws_info.user = this;
    lws_info.count_threads = _impl->service_thread_count;
    if (_impl->page_404_path.empty() == false)
        lws_info.error_document_404 = _impl->page_404_path.c_str();
    if (_impl->lws_mount_ptr != nullptr)
        lws_info.mounts = _impl->lws_mount_ptr;
    if (_impl->ssl_cert_path.empty() == false && _impl->ssl_cert_key.empty() == false)
    {
        lws_info.ssl_cert_filepath = _impl->ssl_cert_path.c_str();
        lws_info.ssl_private_key_filepath = _impl->ssl_cert_key.c_str();
        lws_info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    }

    _impl->lws_context_ptr = lws_create_context(&lws_info);
    if (_impl->lws_context_ptr == nullptr)
    {
        SIHD_LOG(error, "HttpServer: failed to create context");
        return false;
    }

    Defer defer_context([this] {
        std::lock_guard l(_impl->mutex);
        if (_impl->lws_context_ptr != nullptr)
        {
            lws_context_destroy(_impl->lws_context_ptr);
            _impl->lws_context_ptr = nullptr;
        }
    });

    _impl->port = lws_info.port;

    if (_impl->stepworker.start_sync_worker(this->name() + "-callback") == false)
        return false;

    for (size_t tsi = 1; tsi < _impl->service_thread_count; ++tsi)
    {
        _impl->service_threads.emplace_back([this, tsi] {
            while (!_impl->stop)
                lws_service_tsi(_impl->lws_context_ptr, 0, (int)tsi);
        });
    }

    this->service_set_ready();

    while (!_impl->stop)
        lws_service_tsi(_impl->lws_context_ptr, 0, 0);

    _impl->service_threads.clear();
    _impl->stepworker.stop_worker();
    return true;
}

bool HttpServer::get_resource_path(std::string_view path, std::string & res)
{
    if (path.size() > 0 && path[0] == '/')
        path = path.substr(1);

    if (!_impl->root_dir.empty())
    {
        std::string full_path = fs::combine(_impl->root_dir, path);
        if (fs::is_file(full_path))
        {
            res = std::move(full_path);
            return true;
        }
        if (path.empty() || fs::is_dir(full_path))
        {
            std::string index_path = fs::combine(full_path, "index.html");
            if (fs::is_file(index_path))
            {
                res = std::move(index_path);
                return true;
            }
        }
    }

    for (const std::string & resource_path : _impl->resources_path)
    {
        std::string tmp_path = fs::combine(resource_path, path);
        if (fs::is_file(tmp_path))
        {
            res = std::move(tmp_path);
            return true;
        }
    }

    return false;
}

bool HttpServer::add_websocket(const char *name, IWebsocketHandler *handler, size_t tx_packet_size)
{
    bool ret = _impl->add_protocol(name,
                                   Impl::_global_websocket_lws_callback,
                                   sizeof(Impl::WebsocketSession),
                                   tx_packet_size);
    if (ret)
    {
        _impl->websocket_handler_lst.resize(_impl->protocols_count);
        _impl->websocket_handler_lst[_impl->protocols_count - 1] = handler;
    }
    return ret;
}

} // namespace sihd::http
