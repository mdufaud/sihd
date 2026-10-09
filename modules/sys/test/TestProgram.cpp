#include <gtest/gtest.h>

#include <sihd/sys/program.hpp>
#include <sihd/util/build.hpp>

using enum sihd::util::ErrorCode;

namespace test
{
using namespace sihd::sys;

class TestProgram: public ::testing::Test
{
    protected:
        TestProgram() = default;
        virtual ~TestProgram() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestProgram, test_program_find_symbol)
{
    if constexpr (sihd::util::build::is_statically_linked || sihd::util::build::is_emscripten)
        GTEST_SKIP() << "global symbol search not available in this build";

    auto sym = program::find_symbol("printf");
    ASSERT_TRUE(sym.has_value());
    EXPECT_NE(*sym, nullptr);

    auto missing = program::find_symbol("nonexistent_symbol_12345");
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, not_found);
    EXPECT_FALSE(missing.error().message.empty());
}

} // namespace test
