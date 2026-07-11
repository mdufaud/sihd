#include <chrono>
#include <memory>
#include <optional>
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
#include <sihd/http/Navigator.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>

namespace sihd::lua
{

using namespace sihd::http;
using sihd::util::Node;
using sihd::util::SmartNodePtr;

namespace
{

// Marshal a NavigatorResponse to a Lua table so scripts never touch the
// move-only C++ type. Returns nil when the request failed (nullopt).
luabridge::LuaRef response_to_lua(lua_State *state, std::optional<NavigatorResponse> && resp)
{
    if (resp.has_value() == false)
        return luabridge::LuaRef(state, luabridge::LuaNil());

    luabridge::LuaRef table = luabridge::newTable(state);
    table["status"] = resp->status();
    table["content"] = resp->content().cpp_str();
    table["final_url"] = resp->final_url();

    luabridge::LuaRef headers = luabridge::newTable(state);
    for (const auto & [name, value] : resp->http_header().headers())
        headers[name] = value;
    table["headers"] = headers;

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
        .addFunction("get",
                     +[](Navigator *self, const std::string & url, lua_State *state) -> luabridge::LuaRef {
                         return response_to_lua(state, self->get(url));
                     })
        .addFunction("post",
                     +[](Navigator *self, const std::string & url, luabridge::LuaRef data, lua_State *state)
                         -> luabridge::LuaRef {
                         const std::string body = data.isNil() ? std::string() : data.tostring();
                         return response_to_lua(state, self->post(url, body));
                     })
        .addFunction("put",
                     +[](Navigator *self, const std::string & url, const std::string & data, lua_State *state)
                         -> luabridge::LuaRef { return response_to_lua(state, self->put(url, data)); })
        .addFunction("patch",
                     +[](Navigator *self, const std::string & url, const std::string & data, lua_State *state)
                         -> luabridge::LuaRef { return response_to_lua(state, self->patch(url, data)); })
        .addFunction("del",
                     +[](Navigator *self, const std::string & url, lua_State *state) -> luabridge::LuaRef {
                         return response_to_lua(state, self->del(url));
                     })
        .addFunction("head",
                     +[](Navigator *self, const std::string & url, lua_State *state) -> luabridge::LuaRef {
                         return response_to_lua(state, self->head(url));
                     })
        .addFunction("options",
                     +[](Navigator *self, const std::string & url, lua_State *state) -> luabridge::LuaRef {
                         return response_to_lua(state, self->options(url));
                     })
        .addFunction("download",
                     +[](Navigator *self, const std::string & url, const std::string & path) -> bool {
                         return self->download(url, path).has_value();
                     })
        // configuration
        .addFunction("set_verbose", &Navigator::set_verbose)
        .addFunction("set_follow_redirects", &Navigator::set_follow_redirects)
        .addFunction("set_max_redirects", &Navigator::set_max_redirects)
        .addFunction("set_timeout", &Navigator::set_timeout)
        .addFunction("set_connect_timeout", &Navigator::set_connect_timeout)
        .addFunction("set_accept_encoding", &Navigator::set_accept_encoding)
        .addFunction("set_http2", &Navigator::set_http2)
        .addFunction("set_ssl_verify", &Navigator::set_ssl_verify)
        .addFunction("set_user_agent",
                     +[](Navigator *self, const std::string & agent) { self->set_user_agent(agent); })
        .addFunction("set_max_response_size",
                     +[](Navigator *self, int bytes) { self->set_max_response_size(static_cast<size_t>(bytes)); })
        .addFunction("set_ssrf_guard", &Navigator::set_ssrf_guard)
        // authentication
        .addFunction("set_basic_auth",
                     +[](Navigator *self, const std::string & user, const std::string & password) {
                         self->set_basic_auth(user, password);
                     })
        .addFunction("set_digest_auth",
                     +[](Navigator *self, const std::string & user, const std::string & password) {
                         self->set_digest_auth(user, password);
                     })
        .addFunction("set_token_auth",
                     +[](Navigator *self, const std::string & token) { self->set_token_auth(token); })
        .addFunction("clear_auth", &Navigator::clear_auth)
        // persistent headers
        .addFunction("set_header",
                     +[](Navigator *self, const std::string & name, const std::string & value) {
                         self->set_header(name, value);
                     })
        .addFunction("remove_header",
                     +[](Navigator *self, const std::string & name) { self->remove_header(name); })
        .addFunction("clear_headers", &Navigator::clear_headers)
        // cookies
        .addFunction("cookies",
                     +[](Navigator *self, lua_State *state) -> luabridge::LuaRef {
                         luabridge::LuaRef table = luabridge::newTable(state);
                         for (const auto & [name, value] : self->cookies())
                             table[name] = value;
                         return table;
                     })
        .addFunction("set_cookie",
                     +[](Navigator *self, const std::string & name, const std::string & value, luabridge::LuaRef domain) {
                         self->set_cookie(name, value, domain.isNil() ? "" : domain.tostring());
                     })
        .addFunction("clear_cookies", &Navigator::clear_cookies)
        .addFunction("save_cookies", +[](Navigator *self, const std::string & path) { self->save_cookies(path); })
        .addFunction("load_cookies", +[](Navigator *self, const std::string & path) { self->load_cookies(path); })
        // proxy
        .addFunction("set_proxy", +[](Navigator *self, const std::string & url) { self->set_proxy(url); })
        .addFunction("set_proxy_auth",
                     +[](Navigator *self, const std::string & user, const std::string & password) {
                         self->set_proxy_auth(user, password);
                     })
        .addFunction("clear_proxy", &Navigator::clear_proxy)
        .endClass()
        // --- server side ---
        .beginClass<HttpRequest>("HttpRequest")
        .addFunction("type_str", +[](HttpRequest *self) -> std::string { return self->type_str(); })
        .addFunction("url", +[](HttpRequest *self) -> std::string { return self->url(); })
        .addFunction("client_ip", +[](HttpRequest *self) -> std::string { return self->client_ip(); })
        .addFunction("has_content", &HttpRequest::has_content)
        .addFunction("text", +[](HttpRequest *self) -> std::string { return self->content().cpp_str(); })
        .addFunction("is_authenticated", &HttpRequest::is_authenticated)
        .addFunction("auth_user", +[](HttpRequest *self) -> std::string { return self->auth_user(); })
        .addFunction("path_param",
                     +[](HttpRequest *self, const std::string & name, lua_State *state) -> luabridge::LuaRef {
                         auto v = self->path_param(name);
                         if (v.has_value())
                             return luabridge::LuaRef(state, std::string(*v));
                         return luabridge::LuaRef(state, luabridge::LuaNil());
                     })
        .addFunction("query_param",
                     +[](HttpRequest *self, const std::string & name, lua_State *state) -> luabridge::LuaRef {
                         auto v = self->query_param(name);
                         if (v.has_value())
                             return luabridge::LuaRef(state, std::string(*v));
                         return luabridge::LuaRef(state, luabridge::LuaNil());
                     })
        .addFunction("cookie",
                     +[](HttpRequest *self, const std::string & name, lua_State *state) -> luabridge::LuaRef {
                         auto v = self->cookie(name);
                         if (v.has_value())
                             return luabridge::LuaRef(state, std::string(*v));
                         return luabridge::LuaRef(state, luabridge::LuaNil());
                     })
        .endClass()
        .beginClass<HttpResponse>("HttpResponse")
        .addFunction("set_status", +[](HttpResponse *self, int status) { self->set_status(status); })
        .addFunction("set_plain_content",
                     +[](HttpResponse *self, const std::string & content) -> bool {
                         return self->set_plain_content(content);
                     })
        .addFunction("set_content_type",
                     +[](HttpResponse *self, const std::string & mime) { self->set_content_type(mime); })
        .addFunction("set_cookie",
                     +[](HttpResponse *self, const std::string & name, const std::string & value, luabridge::LuaRef opts) {
                         self->set_cookie(name, value, opts.isNil() ? "" : opts.tostring());
                     })
        .endClass()
        .beginClass<WebService>("WebService")
        .addFunction(
            "set_entry_point",
            // The handler fires on a server worker thread; give it its own Lua
            // coroutine + universe-GIL guard via LuaThreadRunner (as channel
            // observers do), stored in a shared_ptr so route-table copies of the
            // std::function do not touch the Lua ref off-thread.
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
        .addFunction("set_root_dir",
                     +[](HttpServer *self, const std::string & dir) -> bool { return self->set_root_dir(dir); })
        .addFunction("set_server_name",
                     +[](HttpServer *self, const std::string & name) -> bool { return self->set_server_name(name); })
        .addFunction("set_cors_origin",
                     +[](HttpServer *self, const std::string & origin) -> bool { return self->set_cors_origin(origin); })
        .addFunction("set_404_path",
                     +[](HttpServer *self, const std::string & path) -> bool { return self->set_404_path(path); })
        .addFunction("add_web_service",
                     +[](HttpServer *self, const std::string & name) -> WebService * {
                         return self->add_child<WebService>(name);
                     })
        .addFunction("start", +[](HttpServer *self, lua_State *state) -> bool {
            LuaGilRelease release(state);
            return self->start();
        })
        .addFunction("stop", +[](HttpServer *self, lua_State *state) -> bool {
            LuaGilRelease release(state);
            return self->stop();
        })
        .addFunction("request_stop", &HttpServer::request_stop)
        .addFunction("set_service_wait_stop", &HttpServer::set_service_wait_stop)
        .addFunction("is_running", +[](HttpServer *self) -> bool { return self->is_running(); })
        .addFunction("wait_ready",
                     +[](HttpServer *self, int ms, lua_State *state) -> bool {
                         LuaGilRelease release(state);
                         return self->wait_ready(std::chrono::milliseconds(ms));
                     })
        .endClass()
        .endNamespace()
        .endNamespace();
}

} // namespace sihd::lua
