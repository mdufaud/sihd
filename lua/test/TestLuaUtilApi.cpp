#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/FS.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/lua/Vm.hpp>
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

            virtual ~TestLuaUtilApi()
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

    TEST_F(TestLuaUtilApi, test_luautil_node)
    {
        LuaUtilApi::load_base(_vm);
        EXPECT_TRUE(this->do_script("test/lua/util/test_node.lua"));
    }

    TEST_F(TestLuaUtilApi, test_luautil_array)
    {
        LuaUtilApi::load_base(_vm);
        EXPECT_TRUE(this->do_script("test/lua/util/test_array.lua"));
    }

    TEST_F(TestLuaUtilApi, test_luautil_tools)
    {
        LuaUtilApi::load_tools(_vm);
        EXPECT_TRUE(this->do_script("test/lua/util/test_tools.lua"));
    }

    TEST_F(TestLuaUtilApi, test_luautil_process)
    {
        if (OS::is_run_by_valgrind())
            GTEST_SKIP() << "Cannot be run under valgrind";
        LuaUtilApi::load_base(_vm);
        LuaUtilApi::load_process(_vm);
        EXPECT_TRUE(this->do_script("test/lua/util/test_process.lua"));
    }

    TEST_F(TestLuaUtilApi, test_luautil_files)
    {
        LuaUtilApi::load_base(_vm);
        LuaUtilApi::load_files(_vm);
        EXPECT_TRUE(this->do_script("test/lua/util/test_files.lua"));
    }

        TEST_F(TestLuaUtilApi, test_luautil_threading)
    {
        LuaUtilApi::load_base(_vm);
        LuaUtilApi::load_tools(_vm);
        LuaUtilApi::load_threading(_vm);
        EXPECT_TRUE(this->do_script("test/lua/util/test_threading.lua"));
    }
}
