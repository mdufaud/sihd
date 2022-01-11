#ifndef __SIHD_HTTP_HTTPSERVER_HPP__
# define __SIHD_HTTP_HTTPSERVER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/IStoppableRunnable.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/StepWorker.hpp>
# include <sihd/util/Waitable.hpp>
# include <sihd/util/Array.hpp>

# include <sihd/http/Mime.hpp>
# include <sihd/http/IWebsocketHandler.hpp>
# include <sihd/http/HttpRequest.hpp>
# include <sihd/http/HttpResponse.hpp>
# include <sihd/http/HttpHeader.hpp>

# include <mutex>
# include <set>
# include <libwebsockets.h>

namespace sihd::http
{

LOGGER;

class HttpServer:   public sihd::util::Named,
                    public sihd::util::Configurable,
                    public sihd::util::IRunnable
{
    public:
        HttpServer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~HttpServer();

        bool set_encoding(const std::string & encoding);
        bool set_port(int port);
        bool set_root_dir(const std::string & root_dir);
        bool set_poll_frequency(double freq);
        bool set_ssl_cert_path(const std::string & path);
        bool set_ssl_cert_key(const std::string & path);
        bool add_resource_path(const std::string & path);
        bool remove_resource_path(const std::string & path);

        virtual bool run();
        virtual bool stop();

        virtual bool get_resource_path(const std::string & path, std::string & res);

        const struct lws *current_lws() const { return _lws_current_wsi_ptr; }

    protected:
        struct HttpSession
        {
        };

        struct WebsocketSession
        {
        };

        static int _global_http_lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
        int _lws_http_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

        template <size_t PROTOCOL_INDEX>
        static int _global_websocket_lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
        {
            HttpServer *server = (HttpServer *)lws_context_user(lws_get_context(wsi));
            return server->_lws_websocket_callback<PROTOCOL_INDEX>(wsi, reason, user, in, len);
        }

        template <size_t PROTOCOL_INDEX>
        int _lws_websocket_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
        {
            bool ret = true;
            WebsocketSession *session = (WebsocketSession *)user;
            IWebsocketHandler *handler = _websocket_handler_lst[PROTOCOL_INDEX];
            switch (reason)
            {
                case LWS_CALLBACK_ESTABLISHED:
                {
                    handler->on_open(PROTOCOL_INDEX);
                    break ;
                }
                case LWS_CALLBACK_SERVER_WRITEABLE:
                {
                    sihd::util::ArrByte arr_to_write;
                    LwsWriteProtocol lws_proto;
                    lws_proto.write_protocol = LWS_WRITE_TEXT;
                    ret = handler->on_write(arr_to_write, &lws_proto);
                    if (ret && arr_to_write.size() > 0)
                    {
                        sihd::util::ArrByte buffer;
                        buffer.resize(arr_to_write.size() + LWS_PRE);
                        memcpy(buffer.data() + LWS_PRE, arr_to_write.data(), arr_to_write.size());
                        int wrote = lws_write(wsi, buffer.data() + LWS_PRE, arr_to_write.size(), lws_proto.write_protocol);
                        if (wrote < arr_to_write.size())
                            ret = false;
                    }
                    break ;
                }
                case LWS_CALLBACK_RECEIVE:
                {
                    sihd::util::ArrByte arr;
                    arr.assign_bytes((uint8_t *)in, len);
                    ret = handler->on_read(arr);
                    break ;
                }
                case LWS_CALLBACK_CLOSED:
                {
                    handler->on_close(PROTOCOL_INDEX);
                    break ;
                }
                default:
                    break ;
            }
            return ret ? 0 : -1;
        }

        template <size_t PROTOCOL_INDEX>
        bool _add_websocket(const std::string & name, IWebsocketHandler *handler, size_t tx_packet_size = 0)
        {
            if (PROTOCOL_INDEX == 0)
            {
                LOG(error, "HttpServer: cannot assign protocol index 0 - already used by http server");
                return false;
            }
            bool ret = this->_add_protocol(name, HttpServer::_global_websocket_lws_callback<PROTOCOL_INDEX>,
                                            sizeof(WebsocketSession), tx_packet_size);
            if (ret)
            {
                _websocket_handler_lst.resize(std::max(_protocols_count, PROTOCOL_INDEX));
                _websocket_handler_lst[PROTOCOL_INDEX] = handler;
            }
            return ret;
        }

        virtual bool _add_protocol(const std::string & name, lws_callback_function *callback, size_t struct_size, size_t tx_packet_size = 0);
        virtual bool _check_all_protocols();
        std::vector<std::string> _get_uri_args(struct lws *wsi);

        virtual bool _send_http_headers(struct lws *wsi, HttpHeader & header);
        virtual bool _send_404(struct lws *wsi);
        virtual const char *_get_client_ip(struct lws *wsi);

    private:
        struct LwsPollingScheduler: public sihd::util::IRunnable
        {
            LwsPollingScheduler(HttpServer *server);
            ~LwsPollingScheduler();

            bool run();

            HttpServer *server;
        };

        char _ip_buf[INET6_ADDRSTRLEN];
        bool _running;
        int _port;
        std::string _root_dir;
        std::set<std::string> _resources_path;
        std::string _ssl_cert_path;
        std::string _ssl_cert_key;
        std::mutex _mutex;

        lws_http_mount *_lws_mount_ptr;
        lws_context *_lws_context_ptr;
        size_t _protocols_count;
        lws_protocols *_lws_protocols_ptr;
        struct lws *_lws_current_wsi_ptr;

        std::vector<IWebsocketHandler *> _websocket_handler_lst;
        std::string _encoding;
        HttpHeader _http_header;
        std::string _404_content;

        // mimetype database
        Mime _mime;
        // poll handler
        LwsPollingScheduler _polling_scheduler;
        sihd::util::StepWorker _worker;
        sihd::util::Waitable _waitable;
};

}

#endif