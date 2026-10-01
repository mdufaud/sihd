#include <gtest/gtest.h>

#include <sihd/sys/DynLib.hpp>
#include <sihd/util/build.hpp>

#if defined(__SIHD_WINDOWS__)
# define SIHD_TEST_LIBC "msvcrt.dll"
#else
# define SIHD_TEST_LIBC "libc.so.6"
#endif

using enum sihd::util::ErrorCode;

namespace test
{
using namespace sihd::sys;

class TestDynLib: public ::testing::Test
{
    protected:
        TestDynLib() = default;
        virtual ~TestDynLib() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestDynLib, test_dynlib_open_close)
{
    if constexpr (!DynLib::supported)
        GTEST_SKIP() << "dynamic loading not supported in this build";

    DynLib lib;
    EXPECT_FALSE(lib.is_open());

    // libc should always be available
    EXPECT_TRUE(lib.open(SIHD_TEST_LIBC).has_value());
    EXPECT_TRUE(lib.is_open());
    EXPECT_TRUE(lib.close());
    EXPECT_FALSE(lib.is_open());
}

TEST_F(TestDynLib, test_dynlib_constructor_open)
{
    if constexpr (!DynLib::supported)
        GTEST_SKIP() << "dynamic loading not supported in this build";

    DynLib lib(SIHD_TEST_LIBC);
    EXPECT_TRUE(lib.is_open());
}

TEST_F(TestDynLib, test_dynlib_load_symbol)
{
    if constexpr (!DynLib::supported)
        GTEST_SKIP() << "dynamic loading not supported in this build";

    DynLib lib(SIHD_TEST_LIBC);
    ASSERT_TRUE(lib.is_open());

    auto sym = lib.load("printf");
    ASSERT_TRUE(sym.has_value());
    EXPECT_NE(*sym, nullptr);

    auto missing = lib.load("nonexistent_symbol_12345");
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, not_found);
    EXPECT_FALSE(missing.error().message.empty());
}

TEST_F(TestDynLib, test_dynlib_open_nonexistent)
{
    if constexpr (!DynLib::supported)
        GTEST_SKIP() << "dynamic loading not supported in this build";

    DynLib lib;
    auto opened = lib.open("libnonexistent_12345.so");
    EXPECT_FALSE(opened.has_value());
    // dlerror text classification falls back to io_error under a localized libc
    EXPECT_NE(opened.error().code, none);
    EXPECT_FALSE(opened.error().message.empty());
    EXPECT_FALSE(lib.is_open());
}

} // namespace test
