#ifndef __SIHD_LUA_TEST_LUA_FIXTURE_HPP__
#define __SIHD_LUA_TEST_LUA_FIXTURE_HPP__

#include <string>

#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{

SIHD_LOGGER;

class LuaFixture: public ::testing::Test
{
    protected:
        LuaFixture() { sihd::util::LoggerManager::stream(); }

        ~LuaFixture() override { sihd::util::LoggerManager::clear_loggers(); }

        void SetUp() override
        {
            _vm.new_state();
            ASSERT_NE(_vm.lua_state(), nullptr);
        }

        void TearDown() override { _vm.close_state(); }

        void do_script(const std::string & path)
        {
            SIHD_LOG_INFO("Starting LUA test: {}", path);
            auto res = _vm.do_file(path);
            ASSERT_TRUE(res) << res.error().message;
        }

        sihd::lua::Vm _vm;
};

} // namespace test

#endif
