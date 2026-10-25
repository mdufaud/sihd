#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/lua/wifi/LuaWifiApi.hpp>
#include <sihd/util/Logger.hpp>

#include "../lua_fixture.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaWifiApi: public test::LuaFixture
{
};

TEST_F(TestLuaWifiApi, test_luawifi_base)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaWifiApi::load_base(_vm);
    this->do_script("test/wifi/lua/test_wifi.lua");
}

} // namespace test
