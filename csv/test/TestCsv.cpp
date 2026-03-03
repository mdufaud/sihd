#include <gtest/gtest.h>
#include <sihd/csv/CsvReader.hpp>
#include <sihd/csv/CsvWriter.hpp>
#include <sihd/csv/utils.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>

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

    ASSERT_NO_THROW(utils::write_csv(path, columns, datas));

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

TEST_F(TestCsv, test_csv_from_string_remove_header)
{
    const std::string content = "col1,col2,col3\n"
                                "a,b,c\n"
                                "d,e,f\n";

    auto csv_lines = utils::csv_from_string(content, true);
    ASSERT_EQ(csv_lines.size(), 2u);
    EXPECT_EQ(csv_lines[0], (std::vector<std::string> {"a", "b", "c"}));
    EXPECT_EQ(csv_lines[1], (std::vector<std::string> {"d", "e", "f"}));
}

TEST_F(TestCsv, test_csv_custom_delimiter)
{
    TmpDir tmp_dir;
    std::string path = fs::combine(tmp_dir.path(), "delim.csv");

    {
        CsvWriter writer;
        ASSERT_TRUE(writer.open(path));
        ASSERT_TRUE(writer.set_delimiter(';'));
        EXPECT_TRUE(writer.write_row({"hello", "world", "test"}));
        EXPECT_TRUE(writer.write_row({"1", "2", "3"}));
        EXPECT_TRUE(writer.close());
    }

    auto content = fs::read_all(path);
    ASSERT_TRUE(content);
    EXPECT_EQ(*content, "hello;world;test\n1;2;3\n");

    {
        CsvReader reader;
        ASSERT_TRUE(reader.open(path));
        ASSERT_TRUE(reader.set_delimiter(';'));

        ASSERT_TRUE(reader.read_next());
        EXPECT_EQ(reader.columns(), (std::vector<std::string> {"hello", "world", "test"}));

        ASSERT_TRUE(reader.read_next());
        EXPECT_EQ(reader.columns(), (std::vector<std::string> {"1", "2", "3"}));

        EXPECT_FALSE(reader.read_next());
        EXPECT_TRUE(reader.close());
    }

    // space delimiter
    std::string space_path = fs::combine(tmp_dir.path(), "space.csv");
    {
        CsvWriter writer;
        ASSERT_TRUE(writer.open(space_path));
        ASSERT_TRUE(writer.set_delimiter(' '));
        EXPECT_TRUE(writer.write_row({"col1", "col2"}));
        EXPECT_TRUE(writer.write_row({"a", "b"}));
        EXPECT_TRUE(writer.close());
    }

    {
        CsvReader reader;
        ASSERT_TRUE(reader.open(space_path));
        ASSERT_TRUE(reader.set_delimiter(' '));

        ASSERT_TRUE(reader.read_next());
        EXPECT_EQ(reader.columns(), (std::vector<std::string> {"col1", "col2"}));

        ASSERT_TRUE(reader.read_next());
        EXPECT_EQ(reader.columns(), (std::vector<std::string> {"a", "b"}));

        EXPECT_FALSE(reader.read_next());
    }

    // pipe delimiter
    std::string pipe_path = fs::combine(tmp_dir.path(), "pipe.csv");
    {
        CsvWriter writer;
        ASSERT_TRUE(writer.open(pipe_path));
        ASSERT_TRUE(writer.set_delimiter('|'));
        EXPECT_TRUE(writer.write_row({"x", "y", "z"}));
        EXPECT_TRUE(writer.close());
    }

    {
        CsvReader reader;
        ASSERT_TRUE(reader.open(pipe_path));
        ASSERT_TRUE(reader.set_delimiter('|'));

        ASSERT_TRUE(reader.read_next());
        EXPECT_EQ(reader.columns(), (std::vector<std::string> {"x", "y", "z"}));

        EXPECT_FALSE(reader.read_next());
    }
}

TEST_F(TestCsv, test_csv_custom_commentary)
{
    TmpDir tmp_dir;
    std::string path = fs::combine(tmp_dir.path(), "comment.csv");

    {
        CsvWriter writer;
        ASSERT_TRUE(writer.open(path));
        ASSERT_TRUE(writer.set_commentary(';'));
        EXPECT_TRUE(writer.write_commentary("this is a comment"));
        EXPECT_TRUE(writer.write_row({"a", "b"}));
        EXPECT_TRUE(writer.close());
    }

    auto content = fs::read_all(path);
    ASSERT_TRUE(content);
    EXPECT_EQ(*content, ";this is a comment\na,b\n");

    {
        CsvReader reader;
        ASSERT_TRUE(reader.open(path));
        ASSERT_TRUE(reader.set_commentary(';'));

        ASSERT_TRUE(reader.read_next());
        EXPECT_EQ(reader.columns(), (std::vector<std::string> {"a", "b"}));

        EXPECT_FALSE(reader.read_next());
    }
}

TEST_F(TestCsv, test_csv_error_paths)
{
    // reader: open nonexistent file
    {
        CsvReader reader;
        EXPECT_FALSE(reader.open("/nonexistent/path/file.csv"));
        EXPECT_FALSE(reader.is_open());
        EXPECT_FALSE(reader.read_next());
    }

    // reader: constructor with nonexistent file
    {
        CsvReader reader("/nonexistent/path/file.csv");
        EXPECT_FALSE(reader.is_open());
        EXPECT_FALSE(reader.read_next());
    }

    // reader: columns on closed reader
    {
        CsvReader reader;
        EXPECT_TRUE(reader.columns().empty());
    }

    // writer: open nonexistent directory
    {
        CsvWriter writer;
        EXPECT_FALSE(writer.open("/nonexistent/path/file.csv"));
        EXPECT_FALSE(writer.is_open());
    }

    // writer: constructor with nonexistent directory
    {
        CsvWriter writer("/nonexistent/path/file.csv");
        EXPECT_FALSE(writer.is_open());
    }

    // write_csv: throws on bad path
    {
        using Data = std::tuple<int>;
        std::vector<Data> rows;
        rows.emplace_back(std::make_tuple(1));
        EXPECT_THROW(utils::write_csv("/nonexistent/path/file.csv", {"col"}, rows), std::runtime_error);
    }

    // set_delimiter: reject non-printable
    {
        CsvReader reader;
        EXPECT_FALSE(reader.set_delimiter(0));
        CsvWriter writer;
        EXPECT_FALSE(writer.set_delimiter(0));
    }

    // set_commentary: reject non-printable
    {
        CsvReader reader;
        EXPECT_FALSE(reader.set_commentary(0));
        CsvWriter writer;
        EXPECT_FALSE(writer.set_commentary(0));
    }
}

TEST_F(TestCsv, test_csv_empty_file)
{
    TmpDir tmp_dir;
    std::string path = fs::combine(tmp_dir.path(), "empty.csv");

    ASSERT_TRUE(fs::write(path, ""));

    CsvReader reader;
    ASSERT_TRUE(reader.open(path));
    EXPECT_FALSE(reader.read_next());
    EXPECT_TRUE(reader.close());

    auto csv_data = utils::read_csv(path, false);
    ASSERT_TRUE(csv_data);
    EXPECT_TRUE(csv_data->empty());
}

TEST_F(TestCsv, test_csv_only_comments)
{
    TmpDir tmp_dir;
    std::string path = fs::combine(tmp_dir.path(), "comments.csv");

    ASSERT_TRUE(fs::write(path, "#line1\n#line2\n#line3\n"));

    CsvReader reader;
    ASSERT_TRUE(reader.open(path));
    EXPECT_FALSE(reader.read_next());
    EXPECT_TRUE(reader.close());

    auto csv_data = utils::read_csv(path, false);
    ASSERT_TRUE(csv_data);
    EXPECT_TRUE(csv_data->empty());
}

TEST_F(TestCsv, test_csv_writer_append)
{
    TmpDir tmp_dir;
    std::string path = fs::combine(tmp_dir.path(), "append.csv");

    {
        CsvWriter writer;
        ASSERT_TRUE(writer.open(path));
        EXPECT_TRUE(writer.write_row({"1", "2", "3"}));
        EXPECT_TRUE(writer.close());
    }

    {
        CsvWriter writer;
        ASSERT_TRUE(writer.open(path, true));
        EXPECT_TRUE(writer.write_row({"4", "5", "6"}));
        EXPECT_TRUE(writer.close());
    }

    auto content = fs::read_all(path);
    ASSERT_TRUE(content);
    EXPECT_EQ(*content, "1,2,3\n4,5,6\n");

    CsvReader reader;
    ASSERT_TRUE(reader.open(path));

    ASSERT_TRUE(reader.read_next());
    EXPECT_EQ(reader.columns(), (std::vector<std::string> {"1", "2", "3"}));

    ASSERT_TRUE(reader.read_next());
    EXPECT_EQ(reader.columns(), (std::vector<std::string> {"4", "5", "6"}));

    EXPECT_FALSE(reader.read_next());
}

TEST_F(TestCsv, test_csv_timestamp_numeric)
{
    TmpDir tmp_dir;
    std::string path = fs::combine(tmp_dir.path(), "timestamp.csv");

    ASSERT_TRUE(fs::write(path, "1000000000,hello\n2000000000,world\n"));

    CsvReader reader;
    ASSERT_TRUE(reader.open(path));
    reader.set_timestamp_col(0);

    time_t ts = 0;

    ASSERT_TRUE(reader.read_next());
    EXPECT_EQ(reader.columns(), (std::vector<std::string> {"1000000000", "hello"}));
    ASSERT_TRUE(reader.get_read_timestamp(&ts));
    EXPECT_EQ(ts, 1000000000);

    ASSERT_TRUE(reader.read_next());
    EXPECT_EQ(reader.columns(), (std::vector<std::string> {"2000000000", "world"}));
    ASSERT_TRUE(reader.get_read_timestamp(&ts));
    EXPECT_EQ(ts, 2000000000);

    EXPECT_FALSE(reader.read_next());
}

TEST_F(TestCsv, test_csv_timestamp_not_set)
{
    TmpDir tmp_dir;
    std::string path = fs::combine(tmp_dir.path(), "no_ts.csv");

    ASSERT_TRUE(fs::write(path, "a,b\n"));

    CsvReader reader;
    ASSERT_TRUE(reader.open(path));

    ASSERT_TRUE(reader.read_next());

    time_t ts = 0;
    // no timestamp column set -> returns false
    EXPECT_FALSE(reader.get_read_timestamp(&ts));
}

TEST_F(TestCsv, test_csv_timestamp_out_of_bounds)
{
    TmpDir tmp_dir;
    std::string path = fs::combine(tmp_dir.path(), "ts_oob.csv");

    ASSERT_TRUE(fs::write(path, "a,b\n"));

    CsvReader reader;
    ASSERT_TRUE(reader.open(path));
    reader.set_timestamp_col(5);

    ASSERT_TRUE(reader.read_next());

    time_t ts = 0;
    EXPECT_FALSE(reader.get_read_timestamp(&ts));
}

TEST_F(TestCsv, test_csv_timestamp_invalid_value)
{
    TmpDir tmp_dir;
    std::string path = fs::combine(tmp_dir.path(), "ts_invalid.csv");

    ASSERT_TRUE(fs::write(path, "not_a_number,hello\n"));

    CsvReader reader;
    ASSERT_TRUE(reader.open(path));
    reader.set_timestamp_col(0);

    ASSERT_TRUE(reader.read_next());

    time_t ts = 0;
    EXPECT_FALSE(reader.get_read_timestamp(&ts));
}

TEST_F(TestCsv, test_csv_escape_str)
{
    EXPECT_EQ(utils::escape_str("hello"), "\"hello\"");
    EXPECT_EQ(utils::escape_str(""), "\"\"");
    EXPECT_EQ(utils::escape_str("hello \"world\""), "\"hello \"\"world\"\"\"");
    EXPECT_EQ(utils::escape_str("a\"b\"c"), "\"a\"\"b\"\"c\"");
    EXPECT_EQ(utils::escape_str("\""), "\"\"\"\"");
}

TEST_F(TestCsv, test_csv_writer_col_tracking)
{
    TmpDir tmp_dir;
    std::string path = fs::combine(tmp_dir.path(), "col_track.csv");
    CsvWriter writer;

    ASSERT_TRUE(writer.open(path));
    EXPECT_EQ(writer.current_col(), 0u);
    EXPECT_EQ(writer.current_row(), 0u);

    EXPECT_TRUE(writer.write("a"));
    EXPECT_EQ(writer.current_col(), 1u);

    EXPECT_TRUE(writer.write("b"));
    EXPECT_EQ(writer.current_col(), 2u);

    EXPECT_TRUE(writer.new_row());
    EXPECT_EQ(writer.current_col(), 0u);
    EXPECT_EQ(writer.current_row(), 1u);
    EXPECT_EQ(writer.max_col(), 2u);

    EXPECT_TRUE(writer.close());
}
} // namespace test
