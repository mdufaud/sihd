#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

// clang-format off
#include <sihd/lua/http/LuaHttpApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/lua/core/LuaCoreApi.hpp>
#include <sihd/lua/LuaGil.hpp>
// clang-format on

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/HttpServer.hpp>
#include <sihd/http/Multipart.hpp>
#include <sihd/http/Navigator.hpp>
#include <sihd/http/RequestOptions.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/http/request.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>
#include <sihd/util/str.hpp>

namespace sihd::lua
{

using namespace sihd::http;
using sihd::util::Node;
using sihd::util::SmartNodePtr;
SIHD_LOGGER;

namespace
{

// status + content + headers, shared by every response conversion
luabridge::LuaRef response_table_to_lua(lua_State *state, int status, std::string content, const HttpHeader & header)
{
    luabridge::LuaRef table = luabridge::newTable(state);
    table["status"] = status;
    table["content"] = std::move(content);

    luabridge::LuaRef headers = luabridge::newTable(state);
    for (const auto & [name, value] : header.headers())
    {
        // stored names carry their ':' separator, callers expect the plain name
        std::string_view key = name;
        if (sihd::util::str::ends_with(key, ":"))
            key.remove_suffix(1);
        headers[std::string(key)] = value;
    }
    table["headers"] = headers;

    return table;
}

// Marshals a NavigatorResponse to a Lua table so scripts never touch the move-only C++ type;
// returns nil on failure, the error logged once here at the lua nil boundary
luabridge::LuaRef response_to_lua(lua_State *state, std::expected<NavigatorResponse, sihd::util::Error> && resp)
{
    if (SIHD_UNEXPECTED_LOG(resp))
        return luabridge::LuaRef(state, luabridge::LuaNil());

    luabridge::LuaRef table = response_table_to_lua(state,
                                                    (int)resp->status(),
                                                    resp->content().cpp_str(),
                                                    resp->http_header());
    table["final_url"] = resp->final_url();

    luabridge::LuaRef cookies = luabridge::newTable(state);
    for (const auto & [name, value] : resp->cookies())
        cookies[name] = value;
    table["cookies"] = cookies;

    luabridge::LuaRef redirects = luabridge::newTable(state);
    int idx = 1;
    for (const std::string & url : resp->redirect_history())
        redirects[idx++] = url;
    table["redirect_history"] = redirects;

    return table;
}

luabridge::LuaRef http_response_to_lua(lua_State *state, std::expected<HttpResponse, sihd::util::Error> && resp)
{
    if (SIHD_UNEXPECTED_LOG(resp))
        return luabridge::LuaRef(state, luabridge::LuaNil());

    return response_table_to_lua(state, (int)resp->status(), resp->content().cpp_str(), resp->http_header());
}

// request bodies are strings: reject anything else instead of
// silently sending e.g. "table: 0x7f..." as the payload
std::string to_body_string(luabridge::LuaRef data)
{
    if (data.isNil())
        return std::string();
    if (data.isString() == false && data.isNumber() == false)
        throw std::invalid_argument("http: request body must be a string or nil");
    return data.tostring();
}

RequestOptions to_request_options(luabridge::LuaRef ref)
{
    // a missing parameter is LUA_TNONE, an explicit nil is LUA_TNIL
    if (ref.type() == LUA_TNONE || ref.isNil())
        return RequestOptions::none();
    if (ref.isUserdata() == false)
        throw std::invalid_argument("http: options must be a RequestOptions");
    return ref.cast<RequestOptions>().value();
}

ProxyType to_proxy_type(luabridge::LuaRef type)
{
    if (type.isNil())
        return ProxyType::Http;
    const std::string name = type.tostring();
    if (name == "http")
        return ProxyType::Http;
    if (name == "socks4")
        return ProxyType::Socks4;
    if (name == "socks5")
        return ProxyType::Socks5;
    throw std::invalid_argument(fmt::format("http: unknown proxy type '{}'", name));
}

void set_options_header(RequestOptions & options, const std::string & name, const std::string & value)
{
    options.headers[name] = value;
}

void set_options_parameter(RequestOptions & options, const std::string & name, const std::string & value)
{
    options.parameters[name] = value;
}

void set_options_proxy(RequestOptions & options, const std::string & url, luabridge::LuaRef type)
{
    options.proxy = url;
    options.proxy_type = to_proxy_type(type);
}

} // namespace

void LuaHttpApi::load_all(Vm & vm)
{
    LuaHttpApi::load_base(vm);
}

void LuaHttpApi::load_base(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("http")
        .beginClass<Navigator>("Navigator")
        .addConstructor<void (*)()>()
        // HTTP methods (return a response table or nil)
        .addFunction(
            "get",
            +[](Navigator *self, const std::string & url, lua_State *state) -> luabridge::LuaRef {
                return response_to_lua(state, self->get(url));
            })
        .addFunction(
            "post",
            +[](Navigator *self, const std::string & url, luabridge::LuaRef data, lua_State *state)
                -> luabridge::LuaRef { return response_to_lua(state, self->post(url, to_body_string(data))); })
        .addFunction(
            "put",
            +[](Navigator *self, const std::string & url, const std::string & data, lua_State *state)
                -> luabridge::LuaRef { return response_to_lua(state, self->put(url, data)); })
        .addFunction(
            "patch",
            +[](Navigator *self, const std::string & url, const std::string & data, lua_State *state)
                -> luabridge::LuaRef { return response_to_lua(state, self->patch(url, data)); })
        .addFunction(
            "del",
            +[](Navigator *self, const std::string & url, lua_State *state) -> luabridge::LuaRef {
                return response_to_lua(state, self->del(url));
            })
        .addFunction(
            "head",
            +[](Navigator *self, const std::string & url, lua_State *state) -> luabridge::LuaRef {
                return response_to_lua(state, self->head(url));
            })
        .addFunction(
            "options",
            +[](Navigator *self, const std::string & url, lua_State *state) -> luabridge::LuaRef {
                return response_to_lua(state, self->options(url));
            })
        .addFunction(
            "post_multipart",
            +[](Navigator *self, const std::string & url, const Multipart & multipart, lua_State *state)
                -> luabridge::LuaRef { return response_to_lua(state, self->post_multipart(url, multipart)); })
        .addFunction(
            "download",
            +[](Navigator *self, const std::string & url, const std::string & path, lua_State *state)
                -> luabridge::LuaRef { return response_to_lua(state, self->download(url, path)); })
        .addFunction(
            "put_file",
            +[](Navigator *self, const std::string & url, const std::string & path, lua_State *state)
                -> luabridge::LuaRef { return response_to_lua(state, self->put_file(url, path)); })
        .addFunction("new_connection_count", &Navigator::new_connection_count)
        // configuration
        .addFunction("set_verbose", &Navigator::set_verbose)
        .addFunction("set_follow_redirects", &Navigator::set_follow_redirects)
        .addFunction("set_max_redirects", &Navigator::set_max_redirects)
        .addFunction(
            "set_timeout",
            +[](Navigator *self, luabridge::LuaRef duration) { self->set_timeout(sihd::lua::to_duration(duration)); })
        .addFunction(
            "set_connect_timeout",
            +[](Navigator *self, luabridge::LuaRef duration) {
                self->set_connect_timeout(sihd::lua::to_duration(duration));
            })
        .addFunction("set_accept_encoding", &Navigator::set_accept_encoding)
        .addFunction("set_http2", &Navigator::set_http2)
        .addFunction("set_ssl_verify", &Navigator::set_ssl_verify)
        .addFunction(
            "set_user_agent",
            +[](Navigator *self, const std::string & agent) { self->set_user_agent(agent); })
        .addFunction(
            "set_max_response_size",
            +[](Navigator *self, int bytes) {
                if (bytes < 0)
                    throw std::invalid_argument("http: max response size must be positive");
                self->set_max_response_size(static_cast<size_t>(bytes));
            })
        .addFunction("set_ssrf_guard", &Navigator::set_ssrf_guard)
        // authentication
        .addFunction(
            "set_basic_auth",
            +[](Navigator *self, const std::string & user, const std::string & password) {
                self->set_basic_auth(user, password);
            })
        .addFunction(
            "set_digest_auth",
            +[](Navigator *self, const std::string & user, const std::string & password) {
                self->set_digest_auth(user, password);
            })
        .addFunction(
            "set_token_auth",
            +[](Navigator *self, const std::string & token) { self->set_token_auth(token); })
        .addFunction("clear_auth", &Navigator::clear_auth)
        // persistent headers
        .addFunction(
            "set_header",
            +[](Navigator *self, const std::string & name, const std::string & value) {
                self->set_header(name, value);
            })
        .addFunction(
            "remove_header",
            +[](Navigator *self, const std::string & name) { self->remove_header(name); })
        .addFunction("clear_headers", &Navigator::clear_headers)
        // cookies
        .addFunction(
            "cookies",
            +[](Navigator *self, lua_State *state) -> luabridge::LuaRef {
                luabridge::LuaRef table = luabridge::newTable(state);
                for (const auto & [name, value] : self->cookies())
                    table[name] = value;
                return table;
            })
        .addFunction(
            "set_cookie",
            +[](Navigator *self, const std::string & name, const std::string & value, luabridge::LuaRef domain) {
                self->set_cookie(name, value, domain.isNil() ? "" : domain.tostring());
            })
        .addFunction("clear_cookies", &Navigator::clear_cookies)
        .addFunction(
            "save_cookies",
            +[](Navigator *self, const std::string & path) { self->save_cookies(path); })
        .addFunction(
            "load_cookies",
            +[](Navigator *self, const std::string & path) { self->load_cookies(path); })
        // proxy
        .addFunction(
            "set_proxy",
            +[](Navigator *self, const std::string & url, luabridge::LuaRef type) {
                self->set_proxy(url, to_proxy_type(type));
            })
        .addFunction(
            "set_proxy_auth",
            +[](Navigator *self, const std::string & user, const std::string & password) {
                self->set_proxy_auth(user, password);
            })
        .addFunction("clear_proxy", &Navigator::clear_proxy)
        .endClass()
        // stateless helpers, one connection per call, options in RequestOptions
        .addFunction(
            "get",
            +[](const std::string & url, lua_State *state) -> luabridge::LuaRef {
                return http_response_to_lua(state, get(url));
            },
            +[](const std::string & url, luabridge::LuaRef options_ref, lua_State *state) -> luabridge::LuaRef {
                return http_response_to_lua(state, get(url, to_request_options(options_ref)));
            })
        .addFunction(
            "post",
            +[](const std::string & url, luabridge::LuaRef data, luabridge::LuaRef options_ref, lua_State *state)
                -> luabridge::LuaRef {
                return http_response_to_lua(state, post(url, to_body_string(data), to_request_options(options_ref)));
            })
        .addFunction(
            "put",
            +[](const std::string & url, const std::string & path, luabridge::LuaRef options_ref, lua_State *state)
                -> luabridge::LuaRef {
                return http_response_to_lua(state, put(url, path, to_request_options(options_ref)));
            })
        .addFunction(
            "patch",
            +[](const std::string & url, luabridge::LuaRef data, luabridge::LuaRef options_ref, lua_State *state)
                -> luabridge::LuaRef {
                return http_response_to_lua(state, patch(url, to_body_string(data), to_request_options(options_ref)));
            })
        .addFunction(
            "del",
            +[](const std::string & url, luabridge::LuaRef options_ref, lua_State *state) -> luabridge::LuaRef {
                return http_response_to_lua(state, del(url, to_request_options(options_ref)));
            })
        .addFunction(
            "delete",
            +[](const std::string & url, luabridge::LuaRef options_ref, lua_State *state) -> luabridge::LuaRef {
                return http_response_to_lua(state, del(url, to_request_options(options_ref)));
            })
        .addFunction(
            "head",
            +[](const std::string & url, luabridge::LuaRef options_ref, lua_State *state) -> luabridge::LuaRef {
                return http_response_to_lua(state, head(url, to_request_options(options_ref)));
            })
        .addFunction(
            "options",
            +[](const std::string & url, luabridge::LuaRef req_options, lua_State *state) -> luabridge::LuaRef {
                return http_response_to_lua(state, options(url, to_request_options(req_options)));
            })
        .beginClass<Multipart>("Multipart")
        .addConstructor<void (*)()>()
        .addFunction(
            "add_field",
            +[](Multipart *self, const std::string & name, const std::string & value) { self->add_field(name, value); })
        .addFunction(
            "add_file",
            +[](Multipart *self, const std::string & name, const std::string & path) { self->add_file(name, path); })
        .addFunction("clear", &Multipart::clear)
        .addFunction("empty", &Multipart::empty)
        .addFunction(
            "value",
            +[](Multipart *self, const std::string & name, lua_State *state) -> luabridge::LuaRef {
                auto value = self->value(name);
                if (value.has_value())
                    return luabridge::LuaRef(state, std::string(*value));
                return luabridge::LuaRef(state, luabridge::LuaNil());
            })
        .endClass()
        .beginClass<RequestOptions>("RequestOptions")
        .addConstructor<void (*)()>()
        .addProperty("verbose", &RequestOptions::verbose)
        .addProperty("follow_location", &RequestOptions::follow_location)
        .addProperty("accept_encoding", &RequestOptions::accept_encoding)
        .addProperty("http2", &RequestOptions::http2)
        .addProperty("ssl_verify_peer", &RequestOptions::ssl_verify_peer)
        .addProperty("ssl_verify_host", &RequestOptions::ssl_verify_host)
        .addProperty("max_response_size", &RequestOptions::max_response_size)
        .addProperty("username", &RequestOptions::username)
        .addProperty("password", &RequestOptions::password)
        .addProperty("digest", &RequestOptions::digest)
        .addProperty("token", &RequestOptions::token)
        .addProperty("user_agent", &RequestOptions::user_agent)
        .addProperty(
            "timeout",
            +[](const RequestOptions *self) { return self->timeout; },
            +[](RequestOptions *self, luabridge::LuaRef value) { self->timeout = sihd::lua::to_duration(value); })
        .addProperty(
            "connect_timeout",
            +[](const RequestOptions *self) { return self->connect_timeout; },
            +[](RequestOptions *self, luabridge::LuaRef value) {
                self->connect_timeout = sihd::lua::to_duration(value);
            })
        .addFunction("set_header", &set_options_header)
        .addFunction("set_parameter", &set_options_parameter)
        .addFunction("set_proxy", &set_options_proxy)
        .addFunction(
            "set_multipart",
            +[](RequestOptions *self, const Multipart & multipart) { self->multipart = multipart; })
        .endClass()
        // --- server side ---
        .beginClass<HttpRequest>("HttpRequest")
        .addFunction(
            "type_str",
            +[](HttpRequest *self) -> std::string { return self->type_str(); })
        .addFunction(
            "url",
            +[](HttpRequest *self) -> std::string { return self->url(); })
        .addFunction(
            "client_ip",
            +[](HttpRequest *self) -> std::string { return self->client_ip(); })
        .addFunction("has_content", &HttpRequest::has_content)
        .addFunction(
            "text",
            +[](HttpRequest *self) -> std::string { return self->content().cpp_str(); })
        .addFunction("is_authenticated", &HttpRequest::is_authenticated)
        .addFunction(
            "auth_user",
            +[](HttpRequest *self) -> std::string { return self->auth_user(); })
        .addFunction(
            "path_param",
            +[](HttpRequest *self, const std::string & name, lua_State *state) -> luabridge::LuaRef {
                auto v = self->path_param(name);
                if (v.has_value())
                    return luabridge::LuaRef(state, std::string(*v));
                return luabridge::LuaRef(state, luabridge::LuaNil());
            })
        .addFunction(
            "query_param",
            +[](HttpRequest *self, const std::string & name, lua_State *state) -> luabridge::LuaRef {
                auto v = self->query_param(name);
                if (v.has_value())
                    return luabridge::LuaRef(state, std::string(*v));
                return luabridge::LuaRef(state, luabridge::LuaNil());
            })
        .addFunction(
            "cookie",
            +[](HttpRequest *self, const std::string & name, lua_State *state) -> luabridge::LuaRef {
                auto v = self->cookie(name);
                if (v.has_value())
                    return luabridge::LuaRef(state, std::string(*v));
                return luabridge::LuaRef(state, luabridge::LuaNil());
            })
        .addFunction("has_multipart", &HttpRequest::has_multipart)
        .addFunction(
            "multipart",
            +[](HttpRequest *self, lua_State *state) -> luabridge::LuaRef {
                const Multipart *multipart = self->multipart();
                if (multipart == nullptr)
                    return luabridge::LuaRef(state, luabridge::LuaNil());
                luabridge::LuaRef parts = luabridge::newTable(state);
                int idx = 1;
                for (const Multipart::Part & part : multipart->parts())
                {
                    luabridge::LuaRef part_table = luabridge::newTable(state);
                    part_table["name"] = part.name;
                    part_table["filename"] = part.filename;
                    part_table["content_type"] = part.content_type;
                    part_table["data"] = part.data;
                    parts[idx++] = part_table;
                }
                return parts;
            })
        .endClass()
        .beginClass<HttpResponse>("HttpResponse")
        .addFunction(
            "set_status",
            +[](HttpResponse *self, int status) { self->set_status(status); })
        .addFunction(
            "set_plain_content",
            +[](HttpResponse *self, const std::string & content) -> bool { return self->set_plain_content(content); })
        .addFunction(
            "set_content_type",
            +[](HttpResponse *self, const std::string & mime) { self->set_content_type(mime); })
        .addFunction(
            "set_cookie",
            +[](HttpResponse *self, const std::string & name, const std::string & value, luabridge::LuaRef opts) {
                self->set_cookie(name, value, opts.isNil() ? "" : opts.tostring());
            })
        .endClass()
        .beginClass<WebService>("WebService")
        .addFunction(
            "set_entry_point",
            // The handler fires on a server worker thread: give it its own LuaThreadRunner (coroutine +
            // universe-GIL guard, like channel observers) in a shared_ptr, safe across route-table copies
            +[](WebService *self,
                const std::string & path,
                luabridge::LuaRef fun,
                luabridge::LuaRef method,
                lua_State *state) {
                if (fun.isFunction() == false)
                {
                    luaL_error(state, "set_entry_point: handler must be a function");
                    return;
                }
                HttpRequest::RequestType type = HttpRequest::Get;
                if (method.isString())
                    type = HttpRequest::type_from_str(method.tostring());
                Vm current_vm(state);
                lua_State *new_thread = current_vm.new_luathread();
                auto runner = std::make_shared<LuaUtilApi::LuaThreadRunner>(fun);
                if (new_thread != nullptr)
                    runner->new_lua_state(new_thread);
                self->set_entry_point(
                    path,
                    [runner](const HttpRequest & req, HttpResponse & resp) {
                        // luabridge pushes registered classes as non-const userdata; the
                        // handler only reads the request, so the const_cast is safe.
                        runner->call_lua_method_noret<HttpRequest *, HttpResponse *>(const_cast<HttpRequest *>(&req),
                                                                                     &resp);
                    },
                    type);
            })
        .endClass()
        .deriveClass<HttpServer, Node>("HttpServer")
        .addConstructorFrom<SmartNodePtr<HttpServer>, void(const std::string &, Node *)>()
        .addFunction("set_port", &HttpServer::set_port)
        .addFunction(
            "set_root_dir",
            +[](HttpServer *self, const std::string & dir) -> bool { return self->set_root_dir(dir); })
        .addFunction(
            "set_server_name",
            +[](HttpServer *self, const std::string & name) -> bool { return self->set_server_name(name); })
        .addFunction(
            "set_cors_origin",
            +[](HttpServer *self, const std::string & origin) -> bool { return self->set_cors_origin(origin); })
        .addFunction(
            "set_404_path",
            +[](HttpServer *self, const std::string & path) -> bool { return self->set_404_path(path); })
        .addFunction(
            "add_web_service",
            +[](HttpServer *self, const std::string & name) -> WebService * {
                return self->add_child<WebService>(name);
            })
        .addFunction(
            "start",
            +[](HttpServer *self, lua_State *state) -> bool {
                LuaGilRelease release(state);
                return self->start();
            })
        .addFunction(
            "stop",
            +[](HttpServer *self, lua_State *state) -> bool {
                LuaGilRelease release(state);
                return self->stop();
            })
        .addFunction("request_stop", &HttpServer::request_stop)
        .addFunction("set_service_wait_stop", &HttpServer::set_service_wait_stop)
        .addFunction(
            "is_running",
            +[](HttpServer *self) -> bool { return self->is_running(); })
        .addFunction(
            "wait_ready",
            +[](HttpServer *self, int ms, lua_State *state) -> bool {
                LuaGilRelease release(state);
                return self->wait_ready(std::chrono::milliseconds(ms));
            })
        .endClass()
        .endNamespace()
        .endNamespace();
}

} // namespace sihd::lua
