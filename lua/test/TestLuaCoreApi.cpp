#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/FS.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/lua/Vm.hpp>
#include <sihd/lua/LuaUtilApi.hpp>
#include <sihd/lua/LuaCoreApi.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::lua;
    using namespace sihd::util;
    class TestLuaCoreApi: public ::testing::Test
    {
        protected:
            TestLuaCoreApi()
            {
                char *test_path = getenv("TEST_PATH");
                _base_test_dir = sihd::util::FS::combine({
                    test_path == nullptr ? "unit_test" : test_path,
                    "lua",
                    "luacoreapi"
                });
                _cwd = sihd::util::OS::cwd();
                sihd::util::LoggerManager::basic();
                sihd::util::FS::make_directories(_base_test_dir);
            }

            virtual ~TestLuaCoreApi()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
                _vm.new_state();
                ASSERT_NE(_vm.lua_state(), nullptr);
            }

            virtual void TearDown()
            {
            }

            bool do_script(const std::string & path)
            {
                bool ret = _vm.do_file(path);
                if (ret == false)
                    SIHD_LOG(error, "Lua error: " << _vm.last_string());
                return ret;
            }

            Vm _vm;
            std::string _cwd;
            std::string _base_test_dir;
    };

    TEST_F(TestLuaCoreApi, test_luacoreapi_channel)
    {
        LuaUtilApi::load_base(_vm);
        LuaUtilApi::load_tools(_vm);
        LuaCoreApi::load(_vm);
        EXPECT_TRUE(this->do_script("test/lua/core/test_channel.lua"));
    }

    TEST_F(TestLuaCoreApi, test_luacoreapi_devpulsation)
    {
        LuaUtilApi::load_base(_vm);
        LuaUtilApi::load_tools(_vm);
        LuaCoreApi::load(_vm);
        EXPECT_TRUE(this->do_script("test/lua/core/test_devpulsation.lua"));
    }
}