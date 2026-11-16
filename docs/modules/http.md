# http

HTTP and WebSocket server built on libwebsockets, and HTTP client functions and a stateful `Navigator` built on `curl`. The server routes requests to `WebService` entry points and supports filters, authentication, streaming bodies and WebSocket endpoints. The client covers one-shot requests, sessions with cookies, proxies, retries and auth, downloads and uploads.

| | |
|---|---|
| Depends on | `curl`, `net` |
| External libraries | libwebsockets, libuv, zlib, openssl |
| Platforms | Linux, Windows (not web or Android) |
| Build | `make m=http` |
| Header | `#include <sihd/http/HttpServer.hpp>` |

## Capabilities

### Server

- `HttpServer`: Node and blocking service that serves HTTP and WebSocket. Configure with `set_port`, `set_interface` (loopback by default, empty for all interfaces), `set_root_dir`, `add_resource_path`, `set_ssl_cert_path`, `set_ssl_cert_key`, `set_cors_origin`, `set_404_path`, `set_server_name`, `set_max_request_size`, `set_max_message_size` (a WebSocket message past it closes its connection with status 1009; 1 MiB by default, 0 for no limit), `set_websocket_max_pending` (messages a WebSocket connection queues before `send` refuses; 1024 by default, 0 for no limit), `set_service_thread_count`, `set_encoding`. Start and stop through the service lifecycle; `request_stop()` ends the loop. `get_port` returns the bound port once ready, the system-picked one when the port is 0.
- `HttpServer::set_http_filter`, `set_authenticator`: install one connection filter and one authenticator.
- `HttpServer::add_websocket<T>(protocol, args...)`: serve a WebSocket protocol, which the client picks with its `Sec-WebSocket-Protocol` name (the path is ignored, an upgrade naming no served protocol is refused). Each connection gets its own `T(IWebsocketConnection &, args...)`; the args are taken by reference and must outlive the server. `add_websocket_origin`: the origins allowed to open them.
- `HttpServer::get_resource_path`: virtual hook that resolves a request path to a file.
- `WebService`: named container of entry points, attached to a server as a child node. `set_entry_point` takes a callable or a member function, with an optional `HttpRequest::RequestType`. `call`, `allowed_methods`, `set_authenticator`, `set_body_stream`.
- `Route`: one path pattern with `{name}` parameter segments and a `{name...}` catch-all segment. `match`, `specificity`, `has_catch_all`.
- `RouteTable`: routes per method. `add`, `find` (returns the handler and path parameters), `allowed_methods`.
- `normalize_path`: strips trailing slash, collapses double slashes and percent-decodes a path.
- `IHttpFilter`, `HttpFilterInfo`: connection-level accept or reject, decided from URI, client IP and headers.
- `RateLimitFilter`: `IHttpFilter` limiting events per client over a window. `set_max_entries`, `set_clock`.
- `IHttpAuthenticator`: override `on_basic_auth` and `on_token_auth` to validate credentials.
- `AWebsocketHandler`: one WebSocket connection, built by the server when the upgrade arrives and destroyed when the connection ends. Its hooks run on the server's service thread, in this order:
  - `on_auth(HttpRequest &)`, before the switch to WebSocket: false refuses the upgrade. The request carries the handshake headers, cookies, query parameters, client IP and the user the server's authenticator accepted; `set_auth_user` there is kept for `request()`.
  - `on_open`.
  - `on_text` (pure) and `on_binary`, each message whole, whatever its fragments and the receive buffer.
  - `on_peer_close(code, reason)` when the client closes, then `on_close`.

  `send` and `send_binary` queue a message from any thread and wake the service at once. They return a `Result`: `would_block` when the queue holds `set_websocket_max_pending` messages (`dropped` counts them), `closed` once closed or closing. `close` sends the queued messages, then closes with status 1000. `is_connected`, `protocol_name`, `request`. A hook that throws closes the connection. The destructor runs on the service thread: another thread keeping a handler pointer removes it there, under the lock it holds while sending.
- `IWebsocketConnection`: the connection the server gives to the handler constructor; `AWebsocketHandler` forwards to it.
- `LoggerWebsocket`: sink pushing one `set_pattern` line (conf key `pattern`, `LogFormatter` syntax, an empty one keeps the default) per record to every connection of the protocol that `serve(server, protocol)` registers, before the server starts. A connection receives the records logged once its upgrade is accepted; inbound messages are ignored. A connection whose queue is full drops the line, and its first drop prints one `LoggerManager::sink_warning` line. `connection_count`. The logger must outlive the server. Built in code next to its `HttpServer`: no factory name, a `CliApp` conf reload would delete it under the server.

### Messages

- `HttpRequest`: parsed request. `RequestType` enum (`Get`, `Post`, `Put`, `Delete`, `Options`, `Patch`, `Head`). Accessors: `path_param`, `query_param`, `cookie`, `content`, `content_as_json`, `has_content`, `http_header`, `client_ip`, `auth_user`, `auth_token`, `is_authenticated`, `multipart`. `from_string` parses a raw request into the request (a failure keeps it) and `to_string` serializes it.
- `HttpResponse`: response under construction. `set_status`, `set_plain_content`, `set_byte_content`, `set_json_content`, `set_file_content` (MIME type from extension), `set_content_type`, `set_cookie`, `set_stream_provider` (chunked producer), `http_header`, `status`, `content`.
- `IBodyStream`, `BodyStreamFunc`, `BodyStreamFile`: receive a request body chunk by chunk instead of buffering it. `BodyStreamFile` writes to a path or a path provider. Attach through `WebService::set_body_stream`.
- `HttpHeader`: header map with typed setters (`set_content_type`, `set_content_length`, `set_server`, `set_header`), `add_header_from_str`, `find`, `get`, `content_length`. `header_param` extracts a parameter from a header value.
- `HttpStatus`: status code constants and helpers `to_string`, `is_redirect`, `is_post_downgrade`, `is_rate_limit`.
- `MimeTypes`: extension to MIME type table; `get`, `add`.
- `Multipart`: multipart body. `parse(body, content_type)` (a failure keeps the parts) or the throwing `Multipart(body, content_type)`, `add_field`, `add_file`, `value`, `file`, `parts`, `boundary`.

### Client functions

- `get`, `post`, `put` (uploads a file), `del`, `options`, `patch`, `head`: one-shot synchronous requests returning `Result<HttpResponse>`.
- `async_get`, `async_post`, `async_put`, `async_del`, `async_options`, `async_patch`, `async_head`: same, returning `std::future`.
- `RequestOptions`: per-request settings: timeouts, `headers`, `parameters`, basic or digest auth, bearer token, proxy (`ProxyType`), `ResolvePin` (dial vetted addresses without DNS), `max_response_size`, multipart, and a `progress` callback (`Progress`). `RequestOptions::none()` returns defaults.
- `scheme_is_ssl`, `scheme_port`: scheme helpers.

### Navigator

- `Navigator`: stateful client, non-copyable. Verbs `get`, `post`, `put`, `patch`, `del`, `head`, `options`, `post_multipart`, `download`, `put_file`.
- `Navigator` session state: `set_header`, `set_cookie` (the domain is required: curl never sends a domain-less cookie to a named host), `cookies`, `save_cookies`, `load_cookies`, `set_basic_auth`, `set_digest_auth`, `set_token_auth`, `form_login` (`FormLoginParams`).
- `Navigator` transport: `set_timeout`, `set_connect_timeout`, `set_ssl_verify`, `set_http2`, `set_max_response_size`, `set_proxy`, `add_proxy`, `set_proxy_rotation` (`ProxyRotation`), `set_retry`, `set_domain_delay_ms`, `set_user_agent_rotation`.
- `Navigator` redirects and safety: `set_follow_redirects`, `set_max_redirects`, `set_redirect_policy` (`RedirectPolicy`), `set_ssrf_guard`, `set_cross_origin_headers`.
- `Navigator` hooks: `on_before_request` (may edit `RequestInfo`), `on_after_response`, `on_auth_required`.
- `Navigator` WebSocket client: `ws_connect` (replaces the current connection), `ws_send`, `ws_send_binary`, `ws_close`, `ws_is_connected`, with `on_ws_open`, `on_ws_text`, `on_ws_binary`, `on_ws_close`, `on_ws_peer_close`.
- `NavigatorResponse`: `HttpResponse` plus `final_url`, `redirect_history`, `cookies`, `was_method_downgraded`, `redirect_refusal`, `truncated`.
- `RobotsTxt`: fetch and parse a `robots.txt`; `is_allowed(user_agent, path)`, `crawl_delay`, TTL with `set_ttl`.
- `extract_links`: links from HTML with optional base URL resolution (`ExtractedLink`). Attribute values are decoded: numeric character references and `&amp;`, `&lt;`, `&gt;`, `&quot;`, `&apos;`; any other reference stays as written, as in `csrf_extract_from_html`.
- `csrf_extract_from_html`: finds a CSRF token field in HTML (`CsrfResult`), optionally guided by a hint.

## Example

Adapted from `modules/http/demo/http_demo.cpp`.

```cpp
#include <sihd/http/HttpServer.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/WebService.hpp>

using namespace sihd::http;

HttpServer server("simple-http-server");
WebService *webservice = server.add_child<WebService>("webservice");
server.set_port(3000);

webservice->set_entry_point("get", [](const HttpRequest & req, HttpResponse & resp) {
    resp.set_plain_content("hello get world");
});

webservice->set_entry_point(
    "post",
    [](const HttpRequest & req, HttpResponse & resp) {
        if (req.has_content())
            resp.set_status(HttpStatus::Ok);
        else
            resp.set_status(HttpStatus::BadRequest);
    },
    HttpRequest::Post);

server.start();
```

`start()` blocks until the service is stopped.

A WebSocket echo, adapted from `modules/http/test/TestWebsocketHandler.cpp`. A browser opens it with `new WebSocket("ws://localhost:3000/", "echo")`.

```cpp
#include <sihd/http/AWebsocketHandler.hpp>
#include <sihd/http/HttpServer.hpp>

using namespace sihd::http;

class Echo: public AWebsocketHandler
{
    public:
        using AWebsocketHandler::AWebsocketHandler;

        void on_text(std::string_view text) override { this->send(text); }
};

HttpServer server("echo-server");
server.add_websocket<Echo>("echo");
```

## Scripting

Exposed to Lua (`LuaHttpApi`, requires `lua=1`) and Python (`sihd.http` submodule, requires `py=1`).
