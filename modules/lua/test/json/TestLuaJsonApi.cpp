#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/json/LuaJsonApi.hpp>
#include <sihd/util/Logger.hpp>

#include "../lua_fixture.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaJsonApi: public test::LuaFixture
{
};

TEST_F(TestLuaJsonApi, test_luajson_base)
{
    LuaJsonApi::load_base(_vm);
    this->do_script("test/json/lua/test_json.lua");
}

} // namespace test
