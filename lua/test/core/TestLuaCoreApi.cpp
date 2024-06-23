#include <gtest/gtest.h>
#include <iostream>
#include <sihd/lua/Vm.hpp>
#include <sihd/lua/core/LuaCoreApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::lua;
using namespace sihd::util;
class TestLuaCoreApi: public ::testing::Test
{
    protected:
        TestLuaCoreApi() { sihd::util::LoggerManager::basic(); }

        virtual ~TestLuaCoreApi() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            _vm.new_state();
            ASSERT_NE(_vm.lua_state(), nullptr);
        }

        virtual void TearDown() {}

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

TEST_F(TestLuaCoreApi, test_luacoreapi_channel)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaCoreApi::load(_vm);
    EXPECT_TRUE(this->do_script("test/core/lua/test_channel.lua"));
}

TEST_F(TestLuaCoreApi, test_luacoreapi_devpulsation)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaCoreApi::load(_vm);
    EXPECT_TRUE(this->do_script("test/core/lua/test_devpulsation.lua"));
}
} // namespace test