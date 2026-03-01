#include <gtest/gtest.h>
#include <sihd/csv/CsvReader.hpp>
#include <sihd/csv/CsvWriter.hpp>
#include <sihd/csv/utils.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::csv;
using namespace sihd::util;
using namespace sihd::sys;
class TestCsv: public ::testing::Test
{
    protected:
        TestCsv() { sihd::util::LoggerManager::stream(); }

        virtual ~TestCsv() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestCsv, test_csv_utils_from_string)
{
    const std::string content = "1,2,,3,4\n"
                                "#commentary !\n"
                                "sup,hello world,bye\n"
                                "\"sup\nnl\",hello\"\"world\"\",bye\"\"\n"
                                "5,6\n"
                                "7.8,9.10\n";
    const std::vector<std::vector<std::string>> expected {
        {"1", "2", "", "3", "4"},
        {"sup", "hello world", "bye"},
        {"sup\nnl", "hello\"world\"", "bye\""},
        {"5", "6"},
        {"7.8", "9.10"},
    };

    auto csv_lines = utils::csv_from_string(content, false, ',', '#');

    for (const auto & columns : csv_lines)
    {
        fmt::print("line: '{}'\n", fmt::join(columns, ","));
    }

    EXPECT_EQ(csv_lines, expected);
}

TEST_F(TestCsv, test_csv_utils_tuple)
{
    using Data = std::tuple<int, std::string, float, char>;

    std::vector<Data> datas;
    datas.emplace_back(std::make_tuple(1, "one", 1.1, '1'));

    const std::vector<std::string> columns = {"number", "string", "float", "char"};

    const std::vector<std::string> expected_line_1 {"1", "one", "1.1", "1"};

    const auto csv_str = utils::csv_to_str(columns, datas);
    EXPECT_EQ(csv_str, "number,string,float,char\n1,one,1.1,1\n");

    TmpDir tmp_dir;
    const std::string path = fmt::format("{}/{}", tmp_dir.path(), "test_tuple.csv");

    ASSERT_TRUE(utils::write_csv(path, columns, datas));

    const auto csv_data = utils::read_csv(path, true);
    ASSERT_TRUE(csv_data);
    ASSERT_EQ(csv_data->size(), 1u);
    EXPECT_EQ(csv_data->at(0), expected_line_1);
}

TEST_F(TestCsv, test_csv_writer)
{
    TmpDir tmp_dir;

    std::string path = fs::combine(tmp_dir.path(), "test_write.csv");
    CsvWriter writer;

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
    EXPECT_TRUE(writer.new_row());
    EXPECT_TRUE(writer.write_row(utils::escape_str("hello \"world\"")));
    EXPECT_TRUE(writer.write_commentary("bye"));
    EXPECT_TRUE(writer.close());

    fmt::print("{}\n", fs::read_all(path).value());

    EXPECT_TRUE(sihd::sys::fs::are_equals(path, "test/resources/expected.csv"));
}

TEST_F(TestCsv, test_csv_reader)
{
    std::string path = "test/resources/to_read.csv";
    CsvReader reader;

    SIHD_LOG(info, "Reading csv: {}", path);
    EXPECT_TRUE(reader.open(path));
    // must skip the comments
    EXPECT_TRUE(reader.read_next());

    // copy values as they will change at next read
    const auto values1 = reader.columns();
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

    // copy values as they will change at next read
    const auto values2 = reader.columns();
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

    // copy values as they will change at next read
    const auto values3 = reader.columns();
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

    auto csv_data = utils::read_csv(path, false);
    ASSERT_TRUE(csv_data);
    ASSERT_EQ(csv_data->size(), 3u);
    EXPECT_EQ(csv_data->at(0), values1);
    EXPECT_EQ(csv_data->at(1), values2);
    EXPECT_EQ(csv_data->at(2), values3);
}
} // namespace test
