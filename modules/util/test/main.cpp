#include <gtest/gtest.h>

#include "watchdog/test_watchdog.hpp"

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    test::install_test_watchdog();
    return RUN_ALL_TESTS();
}