#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/csv/CsvWriter.hpp>
#include <sihd/csv/CsvReader.hpp>
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
        std::string path = Files::combine(_base_test_dir, "test_write.csv");
        CsvWriter writer("csv-writer");

        LOG(info, "Writing csv: " << path);
        EXPECT_TRUE(writer.open(path));
        EXPECT_TRUE(writer.write_commentary(""));
        EXPECT_TRUE(writer.write_commentary("hello world"));
        EXPECT_TRUE(writer.write_row({"1", "2", "3"}));
        EXPECT_TRUE(writer.write("1"));
        EXPECT_TRUE(writer.write("2", 1));
        EXPECT_TRUE(writer.write("3"));
        EXPECT_TRUE(writer.write("4"));
        EXPECT_TRUE(writer.new_row());
        EXPECT_TRUE(writer.write({"1", "2", "3"}));
        EXPECT_TRUE(writer.write_row({"4", "5"}));
        EXPECT_TRUE(writer.write("hello world", strlen("hello world"), 1234));
        EXPECT_TRUE(writer.set_quote_value('"'));
        EXPECT_TRUE(writer.write("hello world", strlen("hello world"), 1234));
        EXPECT_TRUE(writer.set_quote_value('('));
        EXPECT_TRUE(writer.write("hello world", strlen("hello world"), 1234));
        EXPECT_TRUE(writer.new_row());
        EXPECT_TRUE(writer.write_commentary("bye"));
        EXPECT_TRUE(writer.close());

        EXPECT_TRUE(sihd::util::Files::are_equals(path, "test/resources/expected.csv"));
    }

    TEST_F(TestCsv, test_csv_reader)
    {
        std::string path = "test/resources/to_read.csv";
        CsvReader reader("csv-reader");
        std::vector<std::string> values;

        LOG(info, "Reading csv: " << path);
        EXPECT_TRUE(reader.open(path));
        // must skip the two comments
        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(values));

        EXPECT_EQ(values.size(), 5u);
        if (values.size() == 5)
        {
            EXPECT_EQ(values[0], "hello");
            EXPECT_EQ(values[1], "world");
            EXPECT_EQ(values[2], "");
            EXPECT_EQ(values[3], "!");
            // EXPECT_EQ(values[4], "");
        }

        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.get_read_data(values));

        EXPECT_EQ(values.size(), 4u);
        if (values.size() == 4)
        {
            EXPECT_EQ(values[0], "1");
            EXPECT_EQ(values[1], "2");
            EXPECT_EQ(values[2], "3");
            EXPECT_EQ(values[3], "4");
        }

        EXPECT_TRUE(reader.read_next());
        EXPECT_TRUE(reader.set_quote_value('['));
        EXPECT_TRUE(reader.get_read_data(values));

        EXPECT_EQ(values.size(), 4u);
        if (values.size() == 4)
        {
            EXPECT_EQ(values[0], "[the]");
            EXPECT_EQ(values[1], "[fox,' is, here]");
            EXPECT_EQ(values[2], "[,crap,]");
            EXPECT_EQ(values[3], "");
        }
        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());
    }
}