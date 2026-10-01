#include <iostream>

#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/core/LuaCoreApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/term.hpp>

#include "../lua_fixture.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::lua;
using namespace sihd::util;
class TestLuaCoreApi: public test::LuaFixture
{
};

TEST_F(TestLuaCoreApi, test_luacoreapi_channel)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaCoreApi::load(_vm);
    this->do_script("test/core/lua/test_channel.lua");
}

TEST_F(TestLuaCoreApi, test_luacoreapi_observer)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaCoreApi::load(_vm);
    this->do_script("test/core/lua/test_observer.lua");
}

TEST_F(TestLuaCoreApi, test_luacoreapi_errors)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaCoreApi::load(_vm);
    this->do_script("test/core/lua/test_channel_errors.lua");
}

TEST_F(TestLuaCoreApi, test_luacoreapi_channel_waiter)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaCoreApi::load(_vm);
    this->do_script("test/core/lua/test_channel_waiter.lua");
}

TEST_F(TestLuaCoreApi, test_luacoreapi_devices)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaCoreApi::load(_vm);
    this->do_script("test/core/lua/test_devices.lua");
}

TEST_F(TestLuaCoreApi, test_luacoreapi_devpulsation)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaCoreApi::load(_vm);
    this->do_script("test/core/lua/test_devpulsation.lua");
}
} // namespace test