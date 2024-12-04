#include <gtest/gtest.h>
#include <iostream>
#include <sihd/lua/Vm.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaUtilApi: public ::testing::Test
{
    protected:
        TestLuaUtilApi() { sihd::util::LoggerManager::stream(); }

        virtual ~TestLuaUtilApi() { sihd::util::LoggerManager::clear_loggers(); }

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

TEST_F(TestLuaUtilApi, test_luautil_node)
{
    LuaUtilApi::load_base(_vm);
    EXPECT_TRUE(this->do_script("test/util/lua/test_node.lua"));
}

TEST_F(TestLuaUtilApi, test_luautil_array)
{
    LuaUtilApi::load_base(_vm);
    EXPECT_TRUE(this->do_script("test/util/lua/test_array.lua"));
}

TEST_F(TestLuaUtilApi, test_luautil_tools)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    EXPECT_TRUE(this->do_script("test/util/lua/test_tools.lua"));
}

TEST_F(TestLuaUtilApi, test_luautil_process)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_process(_vm);
    // EXPECT_TRUE(this->do_script("test/util/lua/test_process.lua"));
}

TEST_F(TestLuaUtilApi, test_luautil_files)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_files(_vm);
    EXPECT_TRUE(this->do_script("test/util/lua/test_files.lua"));
}

TEST_F(TestLuaUtilApi, test_luautil_thread)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaUtilApi::load_threading(_vm);
    EXPECT_TRUE(this->do_script("test/util/lua/test_thread.lua"));
}

} // namespace test