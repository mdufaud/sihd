#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/core/LuaCoreApi.hpp>
#include <sihd/lua/net/LuaNetApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/util/Logger.hpp>

#include "../lua_fixture.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaNetApi: public test::LuaFixture
{
};

TEST_F(TestLuaNetApi, test_luanet_base)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaCoreApi::load(_vm);
    LuaNetApi::load_base(_vm);
    this->do_script("test/net/lua/test_net.lua");
}

TEST_F(TestLuaNetApi, test_luanet_uncaught_error_keeps_its_code)
{
    LuaUtilApi::load_base(_vm);
    LuaCoreApi::load(_vm);
    LuaNetApi::load_base(_vm);

    auto ran = _vm.do_string("sihd.net.wifi.link('sihd-no-such-interface')");

    ASSERT_FALSE(ran.has_value());
    EXPECT_EQ(ran.error().code, ErrorCode::not_found);
    EXPECT_EQ(ran.error().message, "no interface named sihd-no-such-interface");
}

} // namespace test
