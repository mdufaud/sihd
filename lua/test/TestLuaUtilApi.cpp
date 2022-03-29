#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/lua/LuaUtilApi.hpp>
#include <sol/sol.hpp>

namespace test
{
    SIHD_NEW_LOGGER("test");
    using namespace sihd::lua;
    class TestLuaUtilApi:   public ::testing::Test
    {
        protected:
            TestLuaUtilApi()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestLuaUtilApi()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestLuaUtilApi, test_luautil_node)
    {
        sol::state lua;
        // For print etc...
        lua.open_libraries(sol::lib::base);
        LuaUtilApi::load(lua);
        lua.script_file("test/lua/util/test_node.lua");
    }

    TEST_F(TestLuaUtilApi, test_luautil_array)
    {
        sol::state lua;
        // For print etc...
        lua.open_libraries(sol::lib::base);
        LuaUtilApi::load(lua);
        lua.script_file("test/lua/util/test_array.lua");
    }

    TEST_F(TestLuaUtilApi, test_luautil_tools)
    {
        sol::state lua;
        // For print etc...
        lua.open_libraries(sol::lib::base);
        LuaUtilApi::load(lua);
        lua.script_file("test/lua/util/test_tools.lua");
    }
}
