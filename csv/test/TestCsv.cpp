#include <gtest/gtest.h>
#include <sihd/csv/CsvReader.hpp>
#include <sihd/csv/CsvWriter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/fs.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::csv;
using namespace sihd::util;
class TestCsv: public ::testing::Test
{
    protected:
        TestCsv() { sihd::util::LoggerManager::basic(); }

        virtual ~TestCsv() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestCsv, test_csv_writer)
{
    TmpDir tmp_dir;

    std::string path = fs::combine(tmp_dir.path(), "test_write.csv");
    CsvWriter writer("csv-writer");

    SIHD_LOG(info, "Writing csv: {}", path);
    EXPECT_TRUE(writer.open(path));
    EXPECT_TRUE(writer.write_commentary(""));
    EXPECT_TRUE(writer.write_commentary("hello world"));
    EXPECT_TRUE(writer.write_row({"1", "2", "3"}));
    EXPECT_TRUE(writer.write("1"));
    EXPECT_TRUE(writer.write("2"));
    EXPECT_TRUE(writer.write("3"));
    EXPECT_TRUE(writer.write("4"));
    EXPECT_TRUE(writer.new_row());
    EXPECT_TRUE(writer.write({"1", "2", "3"}));
    EXPECT_TRUE(writer.write_row({"4", "5"}));
    EXPECT_TRUE(writer.write("hello world", 1234));
    EXPECT_TRUE(writer.set_quote_value('"'));
    EXPECT_TRUE(writer.write("hello world", 1234));
    EXPECT_TRUE(writer.set_quote_value('('));
    EXPECT_TRUE(writer.write("hello world", 1234));
    EXPECT_TRUE(writer.new_row());
    EXPECT_TRUE(writer.write_commentary("bye"));
    EXPECT_TRUE(writer.close());

    EXPECT_TRUE(sihd::util::fs::are_equals(path, "test/resources/expected.csv"));
}

TEST_F(TestCsv, test_csv_reader)
{
    std::string path = "test/resources/to_read.csv";
    CsvReader reader;

    SIHD_LOG(info, "Reading csv: {}", path);
    EXPECT_TRUE(reader.open(path));
    // must skip the comments
    EXPECT_TRUE(reader.read_next());

    const auto & values1 = reader.columns();
    fmt::print("first values: '{}'\n", fmt::join(values1, ","));

    EXPECT_EQ(values1.size(), 6u);
    if (values1.size() == 6)
    {
        EXPECT_EQ(values1[0], "hello");
        EXPECT_EQ(values1[1], "world");
        EXPECT_EQ(values1[2], "\"");
        EXPECT_EQ(values1[3], "");
        EXPECT_EQ(values1[4], "!");
        EXPECT_EQ(values1[5], "");
    }

    EXPECT_TRUE(reader.read_next());

    const auto & values2 = reader.columns();
    fmt::print("second values: '{}'\n", fmt::join(values2, ","));

    EXPECT_EQ(values2.size(), 4u);
    if (values2.size() == 4)
    {
        EXPECT_EQ(values2[0], "1");
        EXPECT_EQ(values2[1], "-2");
        EXPECT_EQ(values2[2], "3000");
        EXPECT_EQ(values2[3], "4.2");
    }

    EXPECT_TRUE(reader.read_next());

    const auto & values3 = reader.columns();
    fmt::print("third values: '{}'\n", fmt::join(values3, ","));

    EXPECT_EQ(values3.size(), 4u);
    if (values3.size() == 4)
    {
        EXPECT_EQ(values3[0], "the");
        EXPECT_EQ(values3[1], "multiline, test\n \n# cannot comment here\n is done");
        EXPECT_EQ(values3[2], ",bye,\"");
        EXPECT_EQ(values3[3], "");
    }

    EXPECT_FALSE(reader.read_next());
    EXPECT_TRUE(reader.close());
}
} // namespace test
