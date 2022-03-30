#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/OS.hpp>

#include <sihd/lua/LuaUtilApi.hpp>

namespace test
{
    SIHD_NEW_LOGGER("test");
    using namespace sihd::util;
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

            bool do_script(const std::string & path)
            {
                lua_State* state = luaL_newstate();
                luaL_openlibs(state);

                LuaApi::load(state);
                LuaUtilApi::load(state);

                int ret = luaL_dofile(state, path.c_str());
                if (ret != 0)
                    SIHD_LOG(error, "Error ! " << lua_tostring(state, -1));
                lua_close(state);
                return ret == 0;
            }
    };

    TEST_F(TestLuaUtilApi, test_luautil_node)
    {
        EXPECT_TRUE(this->do_script("test/lua/util/test_node.lua"));
    }

    TEST_F(TestLuaUtilApi, test_luautil_array)
    {
        EXPECT_TRUE(this->do_script("test/lua/util/test_array.lua"));
    }

    TEST_F(TestLuaUtilApi, test_luautil_tools)
    {
        EXPECT_TRUE(this->do_script("test/lua/util/test_tools.lua"));
    }

    TEST_F(TestLuaUtilApi, test_luautil_scheduler)
    {
        EXPECT_TRUE(this->do_script("test/lua/util/test_scheduler.lua"));
    }

    TEST_F(TestLuaUtilApi, test_luautil_process)
    {
        EXPECT_TRUE(this->do_script("test/lua/util/test_process.lua"));
    }
}
