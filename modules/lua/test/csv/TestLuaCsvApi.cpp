#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/csv/LuaCsvApi.hpp>
#include <sihd/util/Logger.hpp>

#include "../lua_fixture.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaCsvApi: public test::LuaFixture
{
};

TEST_F(TestLuaCsvApi, test_luacsv_base)
{
    LuaCsvApi::load_base(_vm);
    this->do_script("test/csv/lua/test_csv.lua");
}

} // namespace test
