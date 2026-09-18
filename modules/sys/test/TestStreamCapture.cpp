#include <cstdio>
#include <iostream>
#include <string>

#include <gtest/gtest.h>

#include <sihd/sys/StreamCapture.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

TEST(TestStreamCapture, test_capture_stdout)
{
    std::string captured;
    {
        StreamCapture cap(stdout);
        std::cout << "hello ";
        std::printf("world\n");
        captured = cap.str();
    }
    EXPECT_EQ(captured, "hello world\n");

    StreamCapture cap(stdout);
    std::printf("restored\n");
    EXPECT_EQ(cap.str(), "restored\n");
}

TEST(TestStreamCapture, test_capture_stderr)
{
    StreamCapture cap(stderr);
    std::cerr << "from cerr\n";
    std::fprintf(stderr, "from fprintf\n");
    EXPECT_EQ(cap.str(), "from cerr\nfrom fprintf\n");
}

TEST(TestStreamCapture, test_capture_nested)
{
    StreamCapture outer(stdout);
    std::printf("outer\n");
    {
        StreamCapture inner(stdout);
        std::printf("inner\n");
        EXPECT_EQ(inner.str(), "inner\n");
    }
    std::printf("outer again\n");
    EXPECT_EQ(outer.str(), "outer\nouter again\n");
}

TEST(TestStreamCapture, test_capture_big_output)
{
    StreamCapture cap(stdout);
    std::string expected;
    for (int i = 0; i < 10000; ++i)
    {
        std::string line = str::format("line {} of a big output past any pipe buffer\n", i);
        expected += line;
        std::fputs(line.c_str(), stdout);
    }
    EXPECT_EQ(cap.str(), expected);
}

} // namespace test
