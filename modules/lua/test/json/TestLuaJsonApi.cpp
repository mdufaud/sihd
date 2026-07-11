#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/json/LuaJsonApi.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaJsonApi: public ::testing::Test
{
    protected:
        TestLuaJsonApi() { sihd::util::LoggerManager::stream(); }

        virtual ~TestLuaJsonApi() { sihd::util::LoggerManager::clear_loggers(); }

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

TEST_F(TestLuaJsonApi, test_luajson_base)
{
    LuaJsonApi::load_base(_vm);
    EXPECT_TRUE(this->do_script("test/json/lua/test_json.lua"));
}

} // namespace test
