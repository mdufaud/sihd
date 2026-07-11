#include <iostream>

#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/sys/LuaSysApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaSysApi: public ::testing::Test
{
    protected:
        TestLuaSysApi() { sihd::util::LoggerManager::stream(); }

        virtual ~TestLuaSysApi() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            _vm.new_state();
            ASSERT_NE(_vm.lua_state(), nullptr);
        }

        virtual void TearDown() { _vm.close_state(); }

        bool do_script(const std::string & path)
        {
            SIHD_LOG_INFO("Starting LUA test: {}", path);
            bool ret = _vm.do_file(path);
            if (ret == false)
                SIHD_LOG(error, "Lua error: {}", _vm.last_string());
            return ret;
        }

        Vm _vm;
};

TEST_F(TestLuaSysApi, test_luasys_uuid_procinfo)
{
    LuaUtilApi::load_base(_vm);
    LuaSysApi::load_base(_vm);
    LuaSysApi::load_tools(_vm);
    EXPECT_TRUE(this->do_script("test/sys/lua/test_uuid_procinfo.lua"));
}

TEST_F(TestLuaSysApi, test_luasys_bitmap)
{
    LuaUtilApi::load_base(_vm);
    LuaSysApi::load_base(_vm);
    EXPECT_TRUE(this->do_script("test/sys/lua/test_bitmap.lua"));
}

TEST_F(TestLuaSysApi, test_luasys_tools)
{
    LuaUtilApi::load_base(_vm);
    LuaSysApi::load_base(_vm);
    LuaSysApi::load_tools(_vm);
    EXPECT_TRUE(this->do_script("test/sys/lua/test_sys_tools.lua"));
}

TEST_F(TestLuaSysApi, test_luasys_process)
{
    LuaUtilApi::load_base(_vm);
    LuaSysApi::load_base(_vm);
    LuaSysApi::load_process(_vm);
    EXPECT_TRUE(this->do_script("test/sys/lua/test_process.lua"));
}

TEST_F(TestLuaSysApi, test_luasys_process_errors)
{
    LuaUtilApi::load_base(_vm);
    LuaSysApi::load_base(_vm);
    LuaSysApi::load_process(_vm);
    EXPECT_TRUE(this->do_script("test/sys/lua/test_process_errors.lua"));
}

TEST_F(TestLuaSysApi, test_luasys_files)
{
    LuaUtilApi::load_base(_vm);
    LuaSysApi::load_base(_vm);
    LuaSysApi::load_files(_vm);
    EXPECT_TRUE(this->do_script("test/sys/lua/test_files.lua"));
}

TEST_F(TestLuaSysApi, test_luasys_proc)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaSysApi::load_base(_vm);
    LuaSysApi::load_process(_vm);
    EXPECT_TRUE(this->do_script("test/sys/lua/test_proc.lua"));
}

} // namespace test
