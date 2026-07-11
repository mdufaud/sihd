#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/core/LuaCoreApi.hpp>
#include <sihd/lua/net/LuaNetApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaNetApi: public ::testing::Test
{
    protected:
        TestLuaNetApi() { sihd::util::LoggerManager::stream(); }

        virtual ~TestLuaNetApi() { sihd::util::LoggerManager::clear_loggers(); }

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

TEST_F(TestLuaNetApi, test_luanet_base)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaCoreApi::load(_vm);
    LuaNetApi::load_base(_vm);
    EXPECT_TRUE(this->do_script("test/net/lua/test_net.lua"));
}

} // namespace test
