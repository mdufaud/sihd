#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/core/LuaCoreApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/lua/zlib/LuaZlibApi.hpp>
#include <sihd/util/Logger.hpp>

#include "../lua_fixture.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaZlibApi: public test::LuaFixture
{
};

TEST_F(TestLuaZlibApi, test_luazlib_base)
{
    LuaUtilApi::load_base(_vm);
    LuaCoreApi::load(_vm);
    LuaZlibApi::load_base(_vm);
    this->do_script("test/zlib/lua/test_zlib.lua");
}

} // namespace test
