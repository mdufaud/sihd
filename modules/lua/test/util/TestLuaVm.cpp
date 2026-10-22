#include <cstdio>
#include <memory>
#include <string>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include "../lua_fixture.hpp"

namespace test
{

namespace
{

struct FileCloser
{
        void operator()(std::FILE *file) const { std::fclose(file); }
};

} // namespace

class TestLuaVm: public LuaFixture
{
};

TEST_F(TestLuaVm, test_luavm_print_stack)
{
    lua_State *state = _vm.lua_state();
    lua_pushinteger(state, 42);
    lua_pushstring(state, "hello");
    lua_pushboolean(state, 1);
    std::unique_ptr<std::FILE, FileCloser> output(std::tmpfile());
    ASSERT_NE(output, nullptr);

    _vm.print_stack(2, output.get());

    std::rewind(output.get());
    std::string printed(256, '\0');
    printed.resize(std::fread(printed.data(), 1, printed.size(), output.get()));
    const std::string expected = fmt::format("lua stack {} (2):\nstack[1]:\t42 (number)\nstack[2]:\thello (string)\n",
                                             static_cast<void *>(state));
    EXPECT_EQ(printed, expected);
}

} // namespace test
