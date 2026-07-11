#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/csv/LuaCsvApi.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaCsvApi: public ::testing::Test
{
    protected:
        TestLuaCsvApi() { sihd::util::LoggerManager::stream(); }

        virtual ~TestLuaCsvApi() { sihd::util::LoggerManager::clear_loggers(); }

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

TEST_F(TestLuaCsvApi, test_luacsv_base)
{
    LuaCsvApi::load_base(_vm);
    EXPECT_TRUE(this->do_script("test/csv/lua/test_csv.lua"));
}

} // namespace test
