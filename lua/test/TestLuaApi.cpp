#include <gtest/gtest.h>
#include <iostream>
#include <sihd/core/Logger.hpp>
#include <sihd/lua/LuaCoreApi.hpp>
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
                sihd::core::LoggerManager::basic();
            }

            virtual ~TestLuaApi()
            {
                sihd::core::LoggerManager::clear_loggers();
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
        lua.script_file("lua/test/lua/test_import.lua");
    }
}
