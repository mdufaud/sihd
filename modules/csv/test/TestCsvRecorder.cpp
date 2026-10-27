#include <regex>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/core/Channel.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevRecorder.hpp>
#include <sihd/csv/CsvRecorder.hpp>
#include <sihd/csv/utils.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::csv;
using namespace sihd::core;
using namespace sihd::util;
using namespace sihd::sys;

class TestCsvRecorder: public ::testing::Test
{
    protected:
        TestCsvRecorder() { LoggerManager::stream(); }

        virtual ~TestCsvRecorder() { LoggerManager::clear_loggers(); }

        std::vector<std::vector<std::string>> rows_of(const std::string & path)
        {
            const std::string content = fs::read_all(path).value_or("");
            return utils::csv_from_string(content, false, ',', '#');
        }

        DevRecorder *add_recorder(Core & core, const std::vector<std::string> & records)
        {
            auto *recorder = core.add_child<DevRecorder>("recorder");
            EXPECT_TRUE(recorder->set_handler("..csv"));
            for (const std::string & record : records)
                EXPECT_TRUE(recorder->add_record_channel(record));
            return recorder;
        }
};

TEST_F(TestCsvRecorder, test_csvrecorder_writes_each_record_at_once)
{
    TmpDir tmp_dir;
    const std::string path = fs::combine(tmp_dir.path(), "records.csv");
    Core core;
    Channel *count = core.add_channel("count", "int", 2);
    Channel *packets = core.add_channel_resizable("packets", "byte", 1, 8);
    auto *csv = core.add_child<CsvRecorder>("csv");
    ASSERT_TRUE(csv->set_path(path));
    this->add_recorder(core, {"count=..count", "packets=..packets"});
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    ASSERT_TRUE(count->write(ArrInt {3, 4}));
    const std::string count_time = std::to_string(count->timestamp().nanoseconds());
    ASSERT_TRUE(packets->write(ArrByte {10, 11, 127}));
    const std::string packets_time = std::to_string(packets->timestamp().nanoseconds());

    const std::vector<std::vector<std::string>> expected = {
        {"timestamp", "channel", "value"},
        {count_time, "count", "3 4"},
        {packets_time, "packets", "0a 0b 7f"},
    };
    EXPECT_EQ(this->rows_of(path), expected);

    ASSERT_TRUE(core.stop());
    EXPECT_FALSE(csv->is_running());
}

TEST_F(TestCsvRecorder, test_csvrecorder_date_in_path)
{
    TmpDir tmp_dir;
    Core core;
    Channel *count = core.add_channel("count", "int", 1);
    auto *csv = core.add_child<CsvRecorder>("csv");
    ASSERT_TRUE(csv->set_path(fs::combine(tmp_dir.path(), "rec_{date}.csv")));
    this->add_recorder(core, {"count=..count"});
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    ASSERT_TRUE(count->write<int32_t>(0, 5));
    ASSERT_TRUE(core.stop());

    const std::vector<std::string> files = fs::children(tmp_dir.path());
    ASSERT_EQ(files.size(), 1u);
    EXPECT_TRUE(std::regex_match(files[0], std::regex(R"(rec_\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}\.csv)")));
    EXPECT_EQ(this->rows_of(fs::combine(tmp_dir.path(), files[0])).size(), 2u);
}

TEST_F(TestCsvRecorder, test_csvrecorder_restart_truncates_the_file)
{
    TmpDir tmp_dir;
    const std::string path = fs::combine(tmp_dir.path(), "records.csv");
    Core core;
    Channel *count = core.add_channel("count", "int", 1);
    auto *csv = core.add_child<CsvRecorder>("csv");
    ASSERT_TRUE(csv->set_path(path));
    this->add_recorder(core, {"count=..count"});
    ASSERT_TRUE(core.init());

    ASSERT_TRUE(core.start());
    ASSERT_TRUE(count->write<int32_t>(0, 1));
    ASSERT_TRUE(core.stop());

    ASSERT_TRUE(core.start());
    ASSERT_TRUE(count->write<int32_t>(0, 2));
    const std::string second_time = std::to_string(count->timestamp().nanoseconds());
    ASSERT_TRUE(core.stop());

    const std::vector<std::vector<std::string>> expected = {
        {"timestamp", "channel", "value"},
        {second_time, "count", "2"},
    };
    EXPECT_EQ(this->rows_of(path), expected);
}

TEST_F(TestCsvRecorder, test_csvrecorder_start_fails_without_path)
{
    CsvRecorder csv("csv");

    EXPECT_FALSE(csv.set_path(""));
    EXPECT_FALSE(csv.start());
    EXPECT_FALSE(csv.is_running());
}

} // namespace test
