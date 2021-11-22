#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/csv/CsvWriter.hpp>
#include <sihd/util/Files.hpp>

namespace test
{
    NEW_LOGGER("test");
    using namespace sihd::csv;
    using namespace sihd::util;
    class TestCsv:   public ::testing::Test
    {
        protected:
            TestCsv()
            {
                sihd::util::LoggerManager::basic();
                Files::make_directories(_base_test_dir);
            }

            virtual ~TestCsv()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _base_test_dir = Files::combine({getenv("TEST_PATH"), "csv"});

    };

    TEST_F(TestCsv, test_csv_writer)
    {
        CsvWriter writer("csv-writer");

        std::string path = Files::combine(_base_test_dir, "test_write.csv");
        LOG(info, "Writing csv: " << path);
        EXPECT_TRUE(writer.open(path));
        EXPECT_TRUE(writer.write_commentary("hello world"));
        EXPECT_TRUE(writer.write_row({"1", "2", "3"}));
        EXPECT_TRUE(writer.write_row({"1", "2", "3", "4"}));
        EXPECT_TRUE(writer.write_row({"1", "2", "3", "4", "5"}));
    }
}