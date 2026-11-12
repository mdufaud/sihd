#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/bt/AssignedNumbers.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::bt;
using namespace sihd::util;
namespace fs = sihd::sys::fs;

class TestBtAssignedNumbers: public ::testing::Test
{
    protected:
        TestBtAssignedNumbers() { LoggerManager::stream(); }

        virtual ~TestBtAssignedNumbers() { LoggerManager::clear_loggers(); }

        std::string write_file(const std::string & name, std::string_view content)
        {
            const std::string path = fs::combine(_tmp_dir.path(), name);
            EXPECT_TRUE(fs::write(path, content).has_value());
            return path;
        }

        // the layout of the sig files, with hand written entries
        void load_all(AssignedNumbers & numbers)
        {
            const std::string companies = this->write_file("company_identifiers.yaml",
                                                           "company_identifiers:\n"
                                                           "\n"
                                                           "  - value: 0x0006\n"
                                                           "    name: 'Microsoft'\n"
                                                           "  - value: 0x0ABC\n"
                                                           "    name: 'It''s \"Quoted\"'\n");
            const std::string members = this->write_file("member_uuids.yaml",
                                                         "# header comment\n"
                                                         "uuids:\n"
                                                         " - uuid: 0xFEF3\n"
                                                         "   name: \"Example \\\"Member\\\"\"\n");
            const std::string services = this->write_file("service_uuids.yaml",
                                                          "uuids:\n"
                                                          " - uuid: 0x180D\n"
                                                          "   name: Heart Rate\n"
                                                          "   id: org.bluetooth.service.heart_rate\n");
            ASSERT_TRUE(numbers.load_company_identifiers(companies).has_value());
            ASSERT_TRUE(numbers.load_member_uuids(members).has_value());
            ASSERT_TRUE(numbers.load_service_uuids(services).has_value());
        }

        void expect_load_error(const std::string & name, std::string_view content, std::string_view message)
        {
            const std::string path = this->write_file(name, content);
            AssignedNumbers numbers;
            const Result<void> loaded = numbers.load_member_uuids(path);
            ASSERT_FALSE(loaded.has_value());
            EXPECT_EQ(loaded.error().code, ErrorCode::invalid_argument);
            EXPECT_EQ(loaded.error().message, fmt::format("'{}' {}", path, message));
        }

        sihd::sys::TmpDir _tmp_dir;
};

TEST_F(TestBtAssignedNumbers, test_btassignednumbers_lookup)
{
    AssignedNumbers numbers;
    this->load_all(numbers);

    EXPECT_EQ(numbers.company(0x0006), "Microsoft");
    EXPECT_EQ(numbers.company(0x0abc), "It's \"Quoted\"");
    EXPECT_EQ(numbers.member(0xfef3), "Example \"Member\"");
    EXPECT_EQ(numbers.service(0x180d), "Heart Rate");

    EXPECT_EQ(numbers.company(0x004c), std::nullopt);
    EXPECT_EQ(numbers.service(0xfef3), std::nullopt);
}

TEST_F(TestBtAssignedNumbers, test_btassignednumbers_vendor)
{
    AssignedNumbers numbers;
    this->load_all(numbers);

    ScanResult result;
    result.uuids = {"0000fef3-0000-1000-8000-00805f9b34fb"};
    EXPECT_EQ(numbers.vendor(result), "Example \"Member\"");

    // microsoft 0x0006 beacon id 0x03 swift pair
    result.manufacturer_data[0x0006] = {0x03, 0x00, 0x80};
    EXPECT_EQ(numbers.vendor(result), "Microsoft");
}

TEST_F(TestBtAssignedNumbers, test_btassignednumbers_vendor_without_files)
{
    const AssignedNumbers numbers;

    ScanResult result;
    EXPECT_EQ(numbers.vendor(result), std::nullopt);

    // eddystone url frame 0x10
    result.service_data["0000feaa-0000-1000-8000-00805f9b34fb"] = {0x10};
    EXPECT_EQ(numbers.vendor(result), "Google LLC");

    // apple 0x004c offline finding type 0x12
    result.manufacturer_data[0x004c] = {0x12, 0x19};
    EXPECT_EQ(numbers.vendor(result), "Apple, Inc.");
}

TEST_F(TestBtAssignedNumbers, test_btassignednumbers_malformed)
{
    this->expect_load_error("header.yaml", "company_identifiers:\n", "line 1: expected 'uuids:'");
    // the nested lists of appearance_values.yaml
    this->expect_load_error("nested.yaml",
                            "uuids:\n"
                            " - uuid: 0x1800\n"
                            "   subcategory:\n"
                            "    - value: 0x01\n",
                            "line 3: not a key: value of a flat list entry");
    this->expect_load_error("orphan.yaml", "uuids:\n   name: GAP\n", "line 2: not a key: value of a flat list entry");
    this->expect_load_error("overflow.yaml",
                            "uuids:\n - uuid: 0x10000\n   name: Large\n",
                            "line 2: expected a 16 bits 'uuid' and a 'name'");
    this->expect_load_error("quote.yaml",
                            "uuids:\n - uuid: 0x1800\n   name: 'unclosed\n",
                            "line 2: expected a 16 bits 'uuid' and a 'name'");
    this->expect_load_error("empty.yaml", "# only a comment\nuuids:\n", "holds no 'uuids:' entry");
}

TEST_F(TestBtAssignedNumbers, test_btassignednumbers_failure_keeps_table)
{
    AssignedNumbers numbers;
    this->load_all(numbers);

    EXPECT_FALSE(numbers.load_company_identifiers(fs::combine(_tmp_dir.path(), "missing.yaml")).has_value());
    EXPECT_FALSE(numbers.load_service_uuids(this->write_file("bad.yaml", "uuids:\n - uuid: 0x1800\n")).has_value());

    EXPECT_EQ(numbers.company(0x0006), "Microsoft");
    EXPECT_EQ(numbers.service(0x180d), "Heart Rate");
}

} // namespace test
