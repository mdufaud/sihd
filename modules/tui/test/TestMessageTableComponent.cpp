#include <array>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/core/Channel.hpp>
#include <sihd/tui/MessageTableComponent.hpp>
#include <sihd/util/Logger.hpp>

#include "render_helper.hpp"

namespace test
{

using namespace sihd::util;
using namespace sihd::core;
using namespace sihd::tui;

class TestMessageTableComponent: public ::testing::Test
{
    protected:
        TestMessageTableComponent(): _schema("ap"), _records("records", "byte", size_t {0})
        {
            LoggerManager::stream();
            _schema.add_field<uint8_t>("bssid", 6);
            _schema.add_field<char>("ssid", 8);
            _schema.add_field<int8_t>("signal");
            _schema.finish();
            _records.reserve(4 * _schema.field_byte_size());
            _records.set_resizable(true);
        }

        virtual ~TestMessageTableComponent() { LoggerManager::clear_loggers(); }

        void add_record(uint8_t last_byte, const std::string & ssid, int8_t signal)
        {
            const std::array<uint8_t, 6> bssid = {0x00, 0x11, 0x22, 0x33, 0x44, last_byte};
            std::vector<uint8_t> name(8, 0);
            std::memcpy(name.data(), ssid.data(), ssid.size());
            ASSERT_TRUE(_schema.set_bytes("bssid", bssid));
            ASSERT_TRUE(_schema.set_bytes("ssid", name));
            _schema.set<int8_t>("signal", signal);
            const IArray *bytes = _schema.array();
            _table.insert(_table.end(), bytes->buf(), bytes->buf() + bytes->byte_size());
            ASSERT_TRUE(_records.write(ArrByteView(_table)));
        }

        Message _schema;
        Channel _records;
        std::vector<uint8_t> _table;
};

TEST_F(TestMessageTableComponent, test_message_table_one_row_per_record)
{
    this->add_record(0x01, "home", -40);
    this->add_record(0x02, "cafe", -72);
    MessageTableOptions options;
    options.titles = {{"signal", "dbm"}};
    options.sort_column = 2;
    ftxui::Component table = MessageTableComponent(_records, _schema, std::move(options));

    EXPECT_EQ(render_lines(table->Render(), 32, 4),
              (std::vector<std::string> {"bssid             ssid   dbm ^",
                                         "────────────────────────────────",
                                         "00:11:22:33:44:02 cafe   -72",
                                         "00:11:22:33:44:01 home   -40"}));
}

TEST_F(TestMessageTableComponent, test_message_table_fields_and_formatters)
{
    this->add_record(0x01, "home", -40);
    MessageTableOptions options;
    options.fields = {"signal", "ssid"};
    options.formatters = {
        {"signal", [](const MessageField & field) { return std::to_string(field.read_value<int8_t>(0)) + " dBm"; }}};
    ftxui::Component table = MessageTableComponent(_records, _schema, std::move(options));

    EXPECT_EQ(render_lines(table->Render(), 20, 3)[2], "-40 dBm  home");
}

TEST_F(TestMessageTableComponent, test_message_table_empty_channel)
{
    ftxui::Component table = MessageTableComponent(_records, _schema, {});

    EXPECT_EQ(render_lines(table->Render(), 30, 3)[2], "");
}

} // namespace test
