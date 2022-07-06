#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/GridBuilder.hpp>
#include <sihd/util/TmpDir.hpp>

namespace test
{
    using namespace sihd::util;
    class TestGridBuilder: public ::testing::Test
    {
        protected:
            TestGridBuilder()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestGridBuilder()
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

    TEST_F(TestGridBuilder, test_guibuilder_simple)
    {
        constexpr int max_lines = 100;
        constexpr int max_cols = 150;

        GridBuilder builder;

        builder.set_window_size({
            .max_y = max_lines,
            .max_x = max_cols
        });

        builder.add_subwindow({
            .grid_y = 3,
            .grid_x = 12,
        });

        // new line

        builder.add_subwindow({
            .grid_y = 3,
            .grid_x = 6,
        });

        builder.add_subwindow({
            .grid_y = 3,
            .grid_x = 6,
        });

        // new line

        builder.add_subwindow({
            .grid_y = 12,
            .grid_x = 12,
        });

        auto blocks = builder.build_grid();

        {
            auto & block = blocks.at(0);

            EXPECT_EQ(block.y, 0);
            EXPECT_EQ(block.x, 0);
            EXPECT_EQ(block.max_y, max_lines / 4);
            EXPECT_EQ(block.max_x, max_cols);
        }

        {
            // new line
            auto & block = blocks.at(1);

            EXPECT_EQ(block.y, max_lines / 4);
            EXPECT_EQ(block.x, 0);
            EXPECT_EQ(block.max_y, max_lines / 4);
            EXPECT_EQ(block.max_x, max_cols / 2);
        }

        {
            auto & block = blocks.at(2);

            EXPECT_EQ(block.y, max_lines / 4);
            EXPECT_EQ(block.x, max_cols / 2);
            EXPECT_EQ(block.max_y, max_lines / 4);
            EXPECT_EQ(block.max_x, max_cols / 2);
        }

        {
            // new line
            auto & block = blocks.at(3);

            EXPECT_EQ(block.y, (max_lines / 4) * 2);
            EXPECT_EQ(block.x, 0);
            EXPECT_EQ(block.max_y, max_lines);
            EXPECT_EQ(block.max_x, max_cols);
        }

        SIHD_COUT(GridBuilder::conf_str(blocks));
    }
}