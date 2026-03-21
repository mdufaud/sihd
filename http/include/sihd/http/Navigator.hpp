#ifndef __SIHD_HTTP_NAVIGATOR_HPP__
#define __SIHD_HTTP_NAVIGATOR_HPP__

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>

#include <sihd/http/NavigatorResponse.hpp>

namespace sihd::http
{

class Navigator
{
    public:
        Navigator();
        ~Navigator();

        Navigator(const Navigator &) = delete;
        Navigator & operator=(const Navigator &) = delete;
        Navigator(Navigator &&) = delete;
        Navigator & operator=(Navigator &&) = delete;

        // HTTP methods
        std::optional<NavigatorResponse> get(std::string_view url);
        std::optional<NavigatorResponse> post(std::string_view url, sihd::util::ArrCharView data = {});
        std::optional<NavigatorResponse> put(std::string_view url, sihd::util::ArrCharView data);
        std::optional<NavigatorResponse> del(std::string_view url);
        std::optional<NavigatorResponse> options(std::string_view url);

        // Configuration
        void set_verbose(bool verbose);
        void set_follow_redirects(bool follow);
        void set_max_redirects(long max);
        void set_timeout(long seconds);
        void set_ssl_verify(bool verify_peer, bool verify_host);
        void set_user_agent(std::string_view agent);

        // Authentication (persistent across requests)
        void set_basic_auth(std::string_view user, std::string_view password);
        void set_token_auth(std::string_view token);
        void clear_auth();

        // Persistent headers
        void set_header(std::string_view name, std::string_view value);
        void remove_header(std::string_view name);
        void clear_headers();

        // Cookies (in-memory cookie jar)
        std::map<std::string, std::string> cookies() const;
        void set_cookie(std::string_view name, std::string_view value, std::string_view domain = "");
        void clear_cookies();

        // WebSocket
        bool ws_connect(std::string_view url, std::string_view protocol = "");
        bool ws_send(std::string_view text);
        bool ws_send_binary(sihd::util::ArrByteView data);
        void ws_close();
        bool ws_is_connected() const;

        // WebSocket callbacks
        std::function<void(std::string_view protocol)> on_ws_open;
        std::function<void(std::string_view message)> on_ws_text;
        std::function<void(sihd::util::ArrByteView data)> on_ws_binary;
        std::function<void()> on_ws_close;
        std::function<void(uint16_t code, std::string_view reason)> on_ws_peer_close;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::http

#endif
