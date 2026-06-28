#include <sihd/util/Logger.hpp>

#include "HttpServerImpl.hpp"

namespace sihd::http
{

SIHD_LOGGER;

HttpServer::Impl::Impl(HttpServer *server):
    server(server),
    stop(false),
    port(0),
    lws_mount_ptr(nullptr),
    lws_context_ptr(nullptr),
    protocols_count(0),
    lws_protocols_ptr(nullptr),
    http_filter(nullptr),
    http_authenticator(nullptr),
    polling_scheduler(server)
{
    stepworker.set_runnable(&polling_scheduler);
    stepworker.set_frequency(10);

    default_server_name = server->name();
    default_cors_origin = "*";
    encoding = "utf-8";

    this->add_protocol("http-server", Impl::_global_http_lws_callback, sizeof(HttpSession), 0);
}

HttpServer::Impl::~Impl()
{
    {
        std::lock_guard sl(sessions_mutex);
        for (HttpSession *session : active_sessions)
        {
            session->clean();
            if (lws_context_ptr == nullptr)
                free(session);
        }
        active_sessions.clear();
    }
    std::lock_guard l(mutex);
    if (lws_protocols_ptr != nullptr)
    {
        free(lws_protocols_ptr);
        lws_protocols_ptr = nullptr;
    }
    if (lws_context_ptr != nullptr)
    {
        lws_context_destroy(lws_context_ptr);
        lws_context_ptr = nullptr;
    }
}

bool HttpServer::Impl::add_protocol(const char *name,
                                     lws_callback_function *callback,
                                     size_t struct_size,
                                     size_t tx_packet_size)
{
    ++protocols_count;
    lws_protocols *old_ptr = lws_protocols_ptr;
    lws_protocols_ptr
        = (lws_protocols *)realloc(lws_protocols_ptr, sizeof(lws_protocols) * (protocols_count + 1));
    if (lws_protocols_ptr != nullptr)
    {
        lws_protocols *proto = &lws_protocols_ptr[protocols_count - 1];
        memset(proto, 0, sizeof(lws_protocols));
        memset(proto + 1, 0, sizeof(lws_protocols));
        proto->name = name;
        proto->callback = callback;
        proto->per_session_data_size = struct_size;
        proto->tx_packet_size = tx_packet_size;
        proto->id = protocols_count - 1;
        proto->user = server;
    }
    else
    {
        SIHD_LOG(error, "HttpServer: failed to add protocol: {}", name);
        lws_protocols_ptr = old_ptr;
        --protocols_count;
    }
    return lws_protocols_ptr != nullptr;
}

bool HttpServer::Impl::check_all_protocols()
{
    size_t i = 1;
    while (i < protocols_count)
    {
        lws_callback_on_writable_all_protocol(lws_context_ptr, &lws_protocols_ptr[i]);
        ++i;
    }
    return i > 1;
}

} // namespace sihd::http
