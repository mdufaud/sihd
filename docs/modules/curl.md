# curl

Thin wrapper over libcurl for single transfers: URL and option setting, request and response bodies through callbacks, multipart uploads, cookies and progress reporting. It is the transport used by `http`. Each `Request` owns one libcurl handle and reuses its connection pool across `perform()` calls.

| | |
|---|---|
| Depends on | `util` |
| External libraries | curl |
| Platforms | Linux, Windows (not web or Android) |
| Build | `make m=curl` |
| Header | `#include <sihd/curl/Request.hpp>` |

## Capabilities

### Transfers

- `Request`: one transfer handle. Set options, then call `perform()`; read the result with `response_code()`, `content_type()`, `redirect_url()`, `last_error()`. `reset()` clears options for reuse. Keep it alive between transfers rather than recreating it per transfer, and do not place it at static storage duration.
- `Request::set_url`, `set_verbose`, `set_follow_location`, `set_resolve` (pins host:port to given addresses, no DNS lookup), `set_timeout`, `set_connect_timeout`, `set_user_agent`, `set_ssl_verify`, `set_http_2_tls`, `set_accept_encoding`.
- `Request::set_userpass`, `set_auth` (`Auth::Basic`, `Auth::Digest`, `Auth::Any`).
- `Request::set_proxy` (`Proxy::Http`, `Proxy::Socks4`, `Proxy::Socks5`), `clear_proxy`, `set_proxy_auth`. An empty proxy URL disables the environment proxy.
- `Request::set_http_get`, `set_http_post`, `set_nobody`, `set_custom_request` (arbitrary method).
- `XferProgress`: download and upload totals and current counters in bytes, passed to `set_progress_callback`.

### Bodies and callbacks

- `Request::set_write_callback`, `set_header_callback`: receive response body and header chunks. Returning false aborts the transfer.
- `Request::set_read_callback`: supplies the request body. Return 0 to end the data, or `Request::READ_ABORT` to abort.
- `Request::set_body`: sends a copied in-memory body.
- `Request::set_upload`, `set_infilesize`: upload mode and total upload size.
- `Request::set_progress_callback`: receives `XferProgress`; returning false aborts.

### Headers and multipart

- `HeaderList`: request header lines. `append`, `lines`, `size`, `empty`. Applied with `Request::set_headers`.
- `Mime`: multipart form builder. `add_part()` returns a `Mime::Part` with `name`, `data`, `file`, `filename`, `content_type` setters. Parts are copied when applied with `Request::set_mime`; `clear_mime` removes them.
- `MimePart`: plain data record of one part, exposed by `Mime::parts()`.

### Cookies

- `Request::set_cookie_engine` enables the in-memory cookie jar.
- `Request::add_cookie`, `cookie_list`, `clear_cookies`.
- `Request::save_cookies`, `load_cookies`: persist and restore the jar from a file path.

### Utilities

- `version()`: libcurl version string.
- `resolve_url(base, reference)`: resolves a reference against a base URL, returns `Result<std::string>`.

## Example

Adapted from `modules/curl/test/TestCurl.cpp`.

```cpp
#include <sihd/curl/Request.hpp>

using namespace sihd::curl;

std::string received;

Request request;
auto callback_set = request.set_write_callback([&received](sihd::util::ArrByteView data) {
    received.append((const char *)data.buf(), data.size());
    return true;
});

if (!callback_set || !request.set_url("file:///tmp/transfer.txt"))
    return;

if (!request.perform())
    return;
// received now holds the transferred bytes
```

## Scripting

Not exposed to Lua or Python. Use `http` for the scripting-facing HTTP API.
