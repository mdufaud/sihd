#include <cstdint>
#include <cstdlib>
#include <thread>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/bt/GattServer.hpp>
#include <sihd/lua/Vm.hpp>
#include <sihd/lua/bt/LuaBtApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/sys/env.hpp>
#include <sihd/util/Logger.hpp>

#include "../lua_fixture.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaBtApi: public test::LuaFixture
{
    protected:
        // libdbus reads the bus address once per process: the scripts get a system bus nobody listens on
        void SetUp() override
        {
            test::LuaFixture::SetUp();
            ASSERT_TRUE(sihd::sys::env::set("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent/sihd_lua_bt_socket"));
        }

        void load_apis()
        {
            LuaUtilApi::load_base(_vm);
            LuaUtilApi::load_tools(_vm);
            LuaBtApi::load_base(_vm);
        }
};

TEST_F(TestLuaBtApi, test_luabt_base)
{
    // another suite may have reached the real system bus first: only a fresh process reads the unreachable one
    GTEST_FLAG_SET(death_test_style, "threadsafe");

    EXPECT_EXIT(
        {
            this->load_apis();
            auto res = _vm.do_file("test/bt/lua/test_bt.lua");
            if (!res)
                fmt::print(stderr, "{}\n", res.error().message);
            std::_Exit(res ? EXIT_SUCCESS : EXIT_FAILURE);
        },
        testing::ExitedWithCode(EXIT_SUCCESS),
        "");
}

// the backend calls the handlers from a thread of its own, after the lua objects that set them are gone
TEST_F(TestLuaBtApi, test_luabt_local_characteristic_threads)
{
    using sihd::bt::LocalCharacteristic;

    this->load_apis();
    this->do_script("test/bt/lua/test_bt_callbacks.lua");

    std::vector<LocalCharacteristic> backend;
    for (const char *name : {"working", "raising", "mistyped"})
        backend.push_back(luabridge::getGlobal(_vm.lua_state(), name).cast<LocalCharacteristic>().value());
    auto dropped = _vm.do_string("working = nil raising = nil mistyped = nil collectgarbage()");
    ASSERT_TRUE(dropped) << dropped.error().message;

    const std::vector<uint8_t> accepted = {0x0a, 0x0b};
    const std::vector<uint8_t> refused = {0x01};
    std::vector<std::vector<uint8_t>> reads(3);
    std::vector<bool> accepted_writes(3);
    std::vector<bool> refused_writes(3);
    std::thread thread([&] {
        for (size_t i = 0; i < backend.size(); ++i)
        {
            reads[i] = backend[i].on_read();
            accepted_writes[i] = backend[i].on_write(accepted);
            refused_writes[i] = backend[i].on_write(refused);
        }
        // the last owner of the lua handlers dies here, on a thread that is not the script's
        backend.clear();
    });
    thread.join();

    EXPECT_EQ(reads[0], (std::vector<uint8_t> {0x01, 0x02, 0x03}));
    EXPECT_TRUE(accepted_writes[0]);
    EXPECT_FALSE(refused_writes[0]);
    for (size_t i = 1; i < reads.size(); ++i)
    {
        EXPECT_TRUE(reads[i].empty());
        EXPECT_FALSE(accepted_writes[i]);
        EXPECT_FALSE(refused_writes[i]);
    }
    auto checked = _vm.do_string(R"(
        assert(#backend_reads == 1)
        assert(#backend_writes == 2 and backend_writes[1] == "\10\11" and backend_writes[2] == "\1")
    )");
    EXPECT_TRUE(checked) << checked.error().message;
}

} // namespace test
