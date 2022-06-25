#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/GuiBuilder.hpp>
#include <sihd/util/TmpDir.hpp>

namespace test
{
    using namespace sihd::util;
    class TestGuiBuilder: public ::testing::Test
    {
        protected:
            TestGuiBuilder()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestGuiBuilder()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestGuiBuilder, test_guibuilder)
    {
        constexpr int max_lines = 100;
        constexpr int max_cols = 150;

        GuiBuilder builder;

        builder.set_window_size(max_lines, max_cols);

        auto block = builder.new_child({
            .blocksize_y = 3,
            .blocksize_x = 12,
        });

        EXPECT_EQ(block.y, 0);
        EXPECT_EQ(block.x, 0);
        EXPECT_EQ(block.max_y, max_lines / 4);
        EXPECT_EQ(block.max_x, max_cols);

        // new line

        block = builder.new_child({
            .blocksize_y = 3,
            .blocksize_x = 6,
        });

        EXPECT_EQ(block.y, max_lines / 4);
        EXPECT_EQ(block.x, 0);
        EXPECT_EQ(block.max_y, max_lines / 4);
        EXPECT_EQ(block.max_x, max_cols / 2);

        block = builder.new_child({
            .blocksize_y = 3,
            .blocksize_x = 6,
        });

        EXPECT_EQ(block.y, max_lines / 4);
        EXPECT_EQ(block.x, max_cols / 2);
        EXPECT_EQ(block.max_y, max_lines / 4);
        EXPECT_EQ(block.max_x, max_cols / 2);

        // new line

        block = builder.new_child({
            .blocksize_y = 12,
            .blocksize_x = 12,
        });

        EXPECT_EQ(block.y, (max_lines / 4) * 2);
        EXPECT_EQ(block.x, 0);
        EXPECT_EQ(block.max_y, max_lines);
        EXPECT_EQ(block.max_x, max_cols);

        SIHD_COUT(builder.dump());
    }
}