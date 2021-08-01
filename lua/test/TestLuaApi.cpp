#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/lua/LuaUtilApi.hpp>
#include <sol/sol.hpp>

namespace test
{
    NEW_LOGGER("test");
    using namespace sihd::lua;
    class TestLuaApi:   public ::testing::Test
    {
        protected:
            TestLuaApi()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestLuaApi()
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

    TEST_F(TestLuaApi, test_lua)
    {
        sol::state lua;
        // For print etc...
        lua.open_libraries(sol::lib::base);
        LuaUtilApi::load(lua);
        lua.script_file("lua/test/lua/test_import.lua");
    }
}
