#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/lua/LuaUtilApi.hpp>
#include <sol/sol.hpp>

namespace test
{
    NEW_LOGGER("test");
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

    TEST_F(TestLuaUtilApi, test_lua_util)
    {
        sol::state lua;
        // For print etc...
        lua.open_libraries(sol::lib::base);
        LuaUtilApi::load(lua);
        lua.script_file("test/lua/test_import.lua");
    }
}
