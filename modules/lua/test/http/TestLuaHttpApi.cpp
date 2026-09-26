#include <chrono>
#include <http_test_helpers.hpp>

#include <gtest/gtest.h>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/Multipart.hpp>
#include <sihd/http/request.hpp>
#include <sihd/lua/Vm.hpp>
#include <sihd/lua/core/LuaCoreApi.hpp>
#include <sihd/lua/http/LuaHttpApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::lua;
using namespace sihd::http;

class TestLuaHttpApi: public ::testing::Test
{
    protected:
        TestLuaHttpApi() { sihd::util::LoggerManager::stream(); }

        virtual ~TestLuaHttpApi() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            _server = std::make_unique<EchoServerScope>();
            _server->start(3011);

            _vm.new_state();
            ASSERT_NE(_vm.lua_state(), nullptr);
        }

        virtual void TearDown()
        {
            _vm.close_state();
            _server.reset();
        }

        bool do_script(const std::string & path)
        {
            SIHD_LOG_INFO("Starting LUA test: {}", path);
            bool ret = _vm.do_file(path);
            if (ret == false)
                SIHD_LOG(error, "Lua error: {}", _vm.last_string());
            return ret;
        }

        std::unique_ptr<EchoServerScope> _server;
        Vm _vm;
};

TEST_F(TestLuaHttpApi, test_luahttp_base)
{
    // HttpServer derives Node, so the http bindings need util + core registered first
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaCoreApi::load(_vm);
    LuaHttpApi::load_base(_vm);
    EXPECT_TRUE(this->do_script("test/http/lua/test_http.lua"));
}

TEST_F(TestLuaHttpApi, test_luahttp_server_route)
{
    LuaUtilApi::load_base(_vm);
    LuaCoreApi::load(_vm);
    LuaHttpApi::load_base(_vm);

    // the lua script builds an HttpServer with a lua route handler on port 3021
    ASSERT_TRUE(this->do_script("test/http/lua/test_server.lua"));

    luabridge::LuaRef server_ref = luabridge::getGlobal(_vm.lua_state(), "server");
    auto casted = server_ref.cast<HttpServer *>();
    ASSERT_TRUE(static_cast<bool>(casted));
    HttpServer *lua_server = casted.value();
    ASSERT_NE(lua_server, nullptr);

    Worker worker([lua_server] {
        lua_server->start();
        return true;
    });
    ASSERT_TRUE(worker.start_sync_worker("lua-http-server"));
    ASSERT_TRUE(lua_server->wait_ready(std::chrono::milliseconds(2000)));

    RequestOptions opt;
    opt.proxy = "";
    opt.timeout = sihd::util::time::sec(5);
    auto resp = sihd::http::post("localhost:3021/api/compute", "payload", opt);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), 200u);
    EXPECT_EQ(resp->content().cpp_str(), "from-lua:payload");

    // a lua handler reads a multipart body through the request bindings
    Multipart form;
    form.add_field("field", "lua-multipart-value");
    opt.multipart = form;
    auto upload = sihd::http::post("localhost:3021/api/upload", "", opt);
    ASSERT_TRUE(upload.has_value());
    EXPECT_EQ(upload->status(), 200u);
    EXPECT_EQ(upload->content().cpp_str(), "lua-multipart:lua-multipart-value");

    lua_server->set_service_wait_stop(true);
    lua_server->request_stop();
    lua_server->stop();
    worker.stop_worker();
}

} // namespace test
