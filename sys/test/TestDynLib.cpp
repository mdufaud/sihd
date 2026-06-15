#include <gtest/gtest.h>

#include <sihd/sys/DynLib.hpp>
#include <sihd/util/build.hpp>

#if defined(__SIHD_WINDOWS__)
# define SIHD_TEST_LIBC "msvcrt.dll"
#else
# define SIHD_TEST_LIBC "libc.so.6"
#endif

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

// DynLib::open is a no-op (always false) in fully static builds — nothing to dlopen
#if !defined(SIHD_STATIC)
TEST_F(TestDynLib, test_dynlib_open_close)
{
    DynLib lib;
    EXPECT_FALSE(lib.is_open());

    // libc should always be available
    EXPECT_TRUE(lib.open(SIHD_TEST_LIBC));
    EXPECT_TRUE(lib.is_open());
    EXPECT_TRUE(lib.close());
    EXPECT_FALSE(lib.is_open());
}

TEST_F(TestDynLib, test_dynlib_constructor_open)
{
    DynLib lib(SIHD_TEST_LIBC);
    EXPECT_TRUE(lib.is_open());
}

TEST_F(TestDynLib, test_dynlib_load_symbol)
{
    DynLib lib(SIHD_TEST_LIBC);
    ASSERT_TRUE(lib.is_open());

    void *sym = lib.load("printf");
    EXPECT_NE(sym, nullptr);

    void *bad = lib.load("nonexistent_symbol_12345");
    EXPECT_EQ(bad, nullptr);
}
#endif

TEST_F(TestDynLib, test_dynlib_open_nonexistent)
{
    DynLib lib;
    EXPECT_FALSE(lib.open("libnonexistent_12345.so"));
    EXPECT_FALSE(lib.is_open());
}

} // namespace test
