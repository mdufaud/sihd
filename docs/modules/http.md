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

- `HttpServer`: Node and blocking service that serves HTTP and WebSocket. Configure with `set_port`, `set_interface` (loopback by default, empty for all interfaces), `set_root_dir`, `add_resource_path`, `set_ssl_cert_path`, `set_ssl_cert_key`, `set_cors_origin`, `set_404_path`, `set_server_name`, `set_max_request_size`, `set_service_thread_count`, `set_encoding`. Start and stop through the service lifecycle; `request_stop()` ends the loop.
- `HttpServer::set_http_filter`, `set_authenticator`: install one connection filter and one authenticator.
- `HttpServer::add_websocket`, `add_websocket_origin`: register a WebSocket protocol handler and the origins allowed to open it.
- `HttpServer::get_resource_path`: virtual hook that resolves a request path to a file.
- `WebService`: named container of entry points, attached to a server as a child node. `set_entry_point` takes a callable or a member function, with an optional `HttpRequest::RequestType`. `call`, `allowed_methods`, `set_authenticator`, `set_body_stream`.
- `Route`: one path pattern with `{name}` parameter segments and a `{name...}` catch-all segment. `match`, `specificity`, `has_catch_all`.
- `RouteTable`: routes per method. `add`, `find` (returns the handler and path parameters), `allowed_methods`.
- `normalize_path`: strips trailing slash, collapses double slashes and percent-decodes a path.
- `IHttpFilter`, `HttpFilterInfo`: connection-level accept or reject, decided from URI, client IP and headers.
- `RateLimitFilter`: `IHttpFilter` limiting events per client over a window. `set_max_entries`, `set_clock`.
- `IHttpAuthenticator`: override `on_basic_auth` and `on_token_auth` to validate credentials.
- `IWebsocketHandler`, `WebsocketHandler`: WebSocket callbacks (`on_open`, `on_read`, `on_write`, `on_close`, `on_peer_close`). `WebsocketHandler` exposes them as `std::function` members. `WriteProtocol` selects the frame type on write.

### Messages

- `HttpRequest`: parsed request. `RequestType` enum (`Get`, `Post`, `Put`, `Delete`, `Options`, `Patch`, `Head`). Accessors: `path_param`, `query_param`, `cookie`, `content`, `content_as_json`, `has_content`, `http_header`, `client_ip`, `auth_user`, `auth_token`, `is_authenticated`, `multipart`. `from_string` and `to_string` parse and serialize raw requests.
- `HttpResponse`: response under construction. `set_status`, `set_plain_content`, `set_byte_content`, `set_json_content`, `set_file_content` (MIME type from extension), `set_content_type`, `set_cookie`, `set_stream_provider` (chunked producer), `http_header`, `status`, `content`.
- `IBodyStream`, `BodyStreamFunc`, `BodyStreamFile`: receive a request body chunk by chunk instead of buffering it. `BodyStreamFile` writes to a path or a path provider. Attach through `WebService::set_body_stream`.
- `HttpHeader`: header map with typed setters (`set_content_type`, `set_content_length`, `set_server`, `set_header`), `add_header_from_str`, `find`, `get`, `content_length`. `header_param` extracts a parameter from a header value.
- `HttpStatus`: status code constants and helpers `to_string`, `is_redirect`, `is_post_downgrade`, `is_rate_limit`.
- `MimeTypes`: extension to MIME type table; `get`, `add`.
- `Multipart`: multipart body. `parse` from a body and content type, `add_field`, `add_file`, `value`, `file`, `parts`, `boundary`.

### Client functions

- `get`, `post`, `put` (uploads a file), `del`, `options`, `patch`, `head`: one-shot synchronous requests returning `Result<HttpResponse>`.
- `async_get`, `async_post`, `async_put`, `async_del`, `async_options`, `async_patch`, `async_head`: same, returning `std::future`.
- `RequestOptions`: per-request settings: timeouts, `headers`, `parameters`, basic or digest auth, bearer token, proxy (`ProxyType`), `ResolvePin` (dial vetted addresses without DNS), `max_response_size`, multipart, and a `progress` callback (`Progress`). `RequestOptions::none()` returns defaults.
- `scheme_is_ssl`, `scheme_port`: scheme helpers.

### Navigator

- `Navigator`: stateful client, non-copyable. Verbs `get`, `post`, `put`, `patch`, `del`, `head`, `options`, `post_multipart`, `download`, `put_file`.
- `Navigator` session state: `set_header`, `set_cookie`, `cookies`, `save_cookies`, `load_cookies`, `set_basic_auth`, `set_digest_auth`, `set_token_auth`, `form_login` (`FormLoginParams`).
- `Navigator` transport: `set_timeout`, `set_connect_timeout`, `set_ssl_verify`, `set_http2`, `set_max_response_size`, `set_proxy`, `add_proxy`, `set_proxy_rotation` (`ProxyRotation`), `set_retry`, `set_domain_delay_ms`, `set_user_agent_rotation`.
- `Navigator` redirects and safety: `set_follow_redirects`, `set_max_redirects`, `set_redirect_policy` (`RedirectPolicy`), `set_ssrf_guard`, `set_cross_origin_headers`.
- `Navigator` hooks: `on_before_request` (may edit `RequestInfo`), `on_after_response`, `on_auth_required`.
- `Navigator` WebSocket client: `ws_connect`, `ws_send`, `ws_send_binary`, `ws_close`, `ws_is_connected`, with `on_ws_open`, `on_ws_text`, `on_ws_binary`, `on_ws_close`, `on_ws_peer_close`.
- `NavigatorResponse`: `HttpResponse` plus `final_url`, `redirect_history`, `cookies`, `was_method_downgraded`, `redirect_refusal`, `truncated`.
- `RobotsTxt`: fetch and parse a `robots.txt`; `is_allowed(user_agent, path)`, `crawl_delay`, TTL with `set_ttl`.
- `extract_links`: links from HTML with optional base URL resolution (`ExtractedLink`).
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

## Scripting

Exposed to Lua (`LuaHttpApi`, requires `lua=1`) and Python (`sihd.http` submodule, requires `py=1`).
