#include <chrono>
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <optional>

#include <fmt/format.h>

#include <sihd/util/Logger.hpp>

#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/Navigator.hpp>

namespace demo
{

using namespace sihd::http;

SIHD_NEW_LOGGER("demo");

void demo_http_cookies()
{
    SIHD_LOG(info, "=== HTTP cookie demo: httpbin.org ===");

    Navigator nav;
    nav.set_follow_redirects(true);
    nav.set_ssl_verify(false, false); // skip cert verification for demo

    // httpbin.org sets cookies via redirect: /cookies/set?name=value → /cookies
    auto resp = nav.get("https://httpbin.org/cookies/set?session=hello&user=navigator");
    if (!resp.has_value())
    {
        SIHD_LOG(error, "HTTP GET failed");
        return;
    }
    SIHD_LOG(info, "Status after set: {}", resp->status());

    auto cookies = nav.cookies();
    SIHD_LOG(info, "Cookies in jar ({}): ", cookies.size());
    for (const auto & [name, value] : cookies)
        SIHD_LOG(info, "  {} = {}", name, value);

    // Now request /cookies — httpbin echoes back the cookies it received
    auto check = nav.get("https://httpbin.org/cookies");
    if (check.has_value())
    {
        SIHD_LOG(info, "Status: {}", check->status());
        SIHD_LOG(info, "Response body: {}", check->content().cpp_str());
    }
}

void demo_websocket()
{
    SIHD_LOG(info, "=== WebSocket demo: ws.postman-echo.com ===");

    Navigator nav;
    nav.set_ssl_verify(false, false);

    std::mutex mtx;
    std::condition_variable cv;
    std::optional<std::string> reply;

    nav.on_ws_open = [&](std::string_view protocol) {
        SIHD_LOG(info, "[WS] Connection established via {}", protocol);
    };

    nav.on_ws_text = [&](std::string_view msg) {
        SIHD_LOG(info, "[WS] Received: {}", msg);
        std::lock_guard lock(mtx);
        reply = std::string(msg);
        cv.notify_all();
    };

    nav.on_ws_close = [&]() {
        SIHD_LOG(info, "[WS] Connection closed");
    };

    nav.on_ws_peer_close = [&](uint16_t code, std::string_view reason) {
        SIHD_LOG(info, "[WS] Server closed: code={} reason={}", code, reason);
    };

    if (!nav.ws_connect("wss://ws.postman-echo.com/raw", ""))
    {
        SIHD_LOG(error, "WebSocket connect failed");
        return;
    }

    const std::string_view messages[] = {
        "Hello from sihd Navigator!",
        "Testing WebSocket echo",
        "Goodbye",
    };

    for (std::string_view msg : messages)
    {
        reply.reset();
        SIHD_LOG(info, "[WS] Sending: {}", msg);
        nav.ws_send(msg);

        std::unique_lock lock(mtx);
        if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return reply.has_value(); }))
        {
            SIHD_LOG(warning, "[WS] Timed out waiting for echo");
            break;
        }
        SIHD_LOG(info, "[WS] Echo OK: {}", *reply);
    }

    nav.ws_close();
}

} // namespace demo

int main()
{
    sihd::util::LoggerManager::stream();

    demo::demo_http_cookies();
    demo::demo_websocket();

    sihd::util::LoggerManager::clear_loggers();
    return 0;
}
