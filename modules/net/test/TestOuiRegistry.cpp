#include <array>
#include <stdexcept>
#include <string>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/net/OuiRegistry.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;
namespace fs = sihd::sys::fs;

class TestOuiRegistry: public ::testing::Test
{
    protected:
        TestOuiRegistry() { LoggerManager::stream(); }

        virtual ~TestOuiRegistry() { LoggerManager::clear_loggers(); }

        std::string write_file(const std::string & name, std::string_view content)
        {
            const std::string path = fs::combine(_tmp_dir.path(), name);
            EXPECT_TRUE(fs::write(path, content).has_value());
            return path;
        }

        // an IEEE oui.txt entry: the (hex) line, the (base 16) line and the address lines
        std::string write_registry()
        {
            return this->write_file("oui.txt",
                                    "OUI/MA-L                                                    Organization\n"
                                    "company_id                                                  Organization\n"
                                    "\n"
                                    "00-17-F2   (hex)\t\tApple, Inc.\n"
                                    "0017F2     (base 16)\t\tApple, Inc.\n"
                                    "\t\t\t\t1 Infinite Loop\n"
                                    "\n"
                                    "00-E0-4C   (hex)\t\tREALTEK SEMICONDUCTOR CORP.\r\n"
                                    "\n"
                                    "02-00-00   (hex)\t\tLocal Example\n");
        }

        sihd::sys::TmpDir _tmp_dir;
};

TEST_F(TestOuiRegistry, test_ouiregistry_vendor)
{
    OuiRegistry registry;
    ASSERT_TRUE(registry.load(this->write_registry()).has_value());

    constexpr std::array<uint8_t, 6> apple_address = {0x00, 0x17, 0xf2, 0x01, 0x02, 0x03};
    EXPECT_EQ(registry.vendor(ArrUByteView(apple_address)), "Apple, Inc.");
    EXPECT_EQ(registry.vendor(std::string_view("00:e0:4c:68:00:01")), "REALTEK SEMICONDUCTOR CORP.");
    EXPECT_EQ(registry.vendor(std::string_view("00:E0:4C:68:00:01")), "REALTEK SEMICONDUCTOR CORP.");

    EXPECT_EQ(registry.vendor(std::string_view("00:11:22:33:44:55")), std::nullopt);
}

TEST_F(TestOuiRegistry, test_ouiregistry_not_universal)
{
    const OuiRegistry registry(this->write_registry());

    // the registry holds 02-00-00 but bit 0x02 marks a locally administered address
    EXPECT_EQ(registry.vendor(std::string_view("02:00:00:00:00:01")), std::nullopt);
    // bit 0x01 marks a group address
    EXPECT_EQ(registry.vendor(std::string_view("01:17:f2:00:00:01")), std::nullopt);
}

TEST_F(TestOuiRegistry, test_ouiregistry_malformed_address)
{
    const OuiRegistry registry(this->write_registry());

    EXPECT_EQ(registry.vendor(std::string_view("00:17:f2")), std::nullopt);
    EXPECT_EQ(registry.vendor(std::string_view("00-17-f2-01-02-03")), std::nullopt);
    EXPECT_EQ(registry.vendor(std::string_view("00:17:g2:01:02:03")), std::nullopt);
    constexpr std::array<uint8_t, 2> short_address = {0x00, 0x17};
    EXPECT_EQ(registry.vendor(ArrUByteView(short_address)), std::nullopt);
}

TEST_F(TestOuiRegistry, test_ouiregistry_failure_keeps_registry)
{
    OuiRegistry registry;
    ASSERT_TRUE(registry.load(this->write_registry()).has_value());

    const std::string malformed = this->write_file("malformed.txt", "00-17   (hex)\t\tBroken\n");
    const Result<void> bad_oui = registry.load(malformed);
    ASSERT_FALSE(bad_oui.has_value());
    EXPECT_EQ(bad_oui.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(bad_oui.error().message, fmt::format("'{}': '00-17' is not an oui", malformed));

    const std::string empty = this->write_file("empty.txt", "OUI/MA-L    Organization\n");
    const Result<void> no_oui = registry.load(empty);
    ASSERT_FALSE(no_oui.has_value());
    EXPECT_EQ(no_oui.error().message, fmt::format("'{}' holds no oui", empty));

    EXPECT_FALSE(registry.load(fs::combine(_tmp_dir.path(), "missing.txt")).has_value());

    EXPECT_EQ(registry.vendor(std::string_view("00:17:f2:01:02:03")), "Apple, Inc.");
}

TEST_F(TestOuiRegistry, test_ouiregistry_throwing_constructor)
{
    EXPECT_THROW(OuiRegistry(fs::combine(_tmp_dir.path(), "missing.txt")), std::runtime_error);
}

} // namespace test
