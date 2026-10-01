#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/lua/zip/LuaZipApi.hpp>
#include <sihd/util/Logger.hpp>

#include "../lua_fixture.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaZipApi: public test::LuaFixture
{
};

TEST_F(TestLuaZipApi, test_luazip_base)
{
    LuaUtilApi::load_base(_vm);
    LuaZipApi::load_base(_vm);
    this->do_script("test/zip/lua/test_zip.lua");
}

} // namespace test
