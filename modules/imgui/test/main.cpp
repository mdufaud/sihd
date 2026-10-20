#include <test_watchdog.hpp>

#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    test::install_test_watchdog();
    return RUN_ALL_TESTS();
}