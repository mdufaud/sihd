#include <cstdint>
#include <initializer_list>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/wifi/taxonomy.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::wifi;

class TestTaxonomy: public ::testing::Test
{
    protected:
        TestTaxonomy() { LoggerManager::stream(); }

        virtual ~TestTaxonomy() { LoggerManager::clear_loggers(); }

        static std::vector<uint8_t> element(uint8_t id, const std::vector<uint8_t> & data)
        {
            std::vector<uint8_t> bytes = {id, static_cast<uint8_t>(data.size())};
            bytes.insert(bytes.end(), data.begin(), data.end());
            return bytes;
        }

        static std::vector<uint8_t> join(std::initializer_list<std::vector<uint8_t>> parts)
        {
            std::vector<uint8_t> bytes;
            for (const std::vector<uint8_t> & part : parts)
                bytes.insert(bytes.end(), part.begin(), part.end());
            return bytes;
        }

        static std::vector<uint8_t> ht_body(std::vector<uint8_t> start)
        {
            start.resize(26, 0x00);
            return start;
        }

        static std::string element_signature(const std::vector<uint8_t> & bytes)
        {
            return taxonomy::element_signature(ArrUByteView(bytes));
        }
};

TEST_F(TestTaxonomy, test_taxonomy_reproduces_an_entry_of_the_google_taxonomy)
{
    // the probe request and the association request behind the "Alcatel One Touch Pop Astro" key
    const std::vector<uint8_t> rates = {0x02, 0x04, 0x0b, 0x16, 0x0c, 0x12, 0x18, 0x24};
    const std::vector<uint8_t> probe = join(
        {element(0, {}),
         element(1, rates),
         element(50, {0x30, 0x48, 0x60, 0x6c}),
         element(127, {0x00, 0x00, 0x00, 0x80}),
         element(107, {0x00}),
         element(221, {0x00, 0x50, 0xf2, 0x04, 0x10, 0x23, 0x00, 0x05, '5', '0', '4', '2', 'T'}),
         element(221, {0x50, 0x6f, 0x9a, 0x09}),
         element(221, {0x50, 0x6f, 0x9a, 0x10})});
    const std::vector<uint8_t> assoc = join({element(0, {}),
                                             element(1, rates),
                                             element(50, {0x30, 0x48, 0x60, 0x6c}),
                                             element(45, ht_body({0x72, 0x11, 0x03, 0xff})),
                                             element(48, {0x01, 0x00}),
                                             element(127, {0x01}),
                                             element(221, {0x00, 0x50, 0xf2, 0x02})});

    const Result<std::string> signature = taxonomy::signature(ArrUByteView(probe), ArrUByteView(assoc));

    ASSERT_TRUE(signature.has_value());
    EXPECT_EQ(
        *signature,
        "wifi4|probe:0,1,50,127,107,221(0050f2,4),221(506f9a,9),221(506f9a,16),extcap:00000080,wps:5042T|assoc:0,1,50,45,"
        "48,127,221(0050f2,2),htcap:1172,htagg:03,htmcs:000000ff,extcap:01");
}

TEST_F(TestTaxonomy, test_taxonomy_details_follow_a_fixed_order_whatever_the_elements_order)
{
    const std::vector<uint8_t> probe = join(
        {element(0, {}),
         element(1, {0x02, 0x04}),
         element(45, ht_body({0x63, 0x01, 0x03})),
         element(50, {0x30}),
         element(59, {0x00}),
         element(127, {0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x40, 0x00}),
         element(191, {0xb0, 0x71, 0xd0, 0x33, 0xfa, 0xff, 0x68, 0x01, 0xfa, 0xff, 0x68, 0x01})});

    EXPECT_EQ(
        element_signature(probe),
        "0,1,45,50,59,127,191,htcap:0163,htagg:03,htmcs:00000000,vhtcap:33d071b0,vhtrxmcs:0168fffa,vhttxmcs:0168fffa,"
        "extcap:040000000100004000");
}

TEST_F(TestTaxonomy, test_taxonomy_power_capability_and_wps_model_name)
{
    const std::vector<uint8_t> elements = join(
        {element(33, {0x00, 0x14}), element(33, {0x01}), element(221, {0x00, 0x50, 0xf2, 0x04, 0x10, 0x4a, 0x00,
                                                                       0x01, 0x10, 0x10, 0x23, 0x00, 0x07, 'N',
                                                                       'e',  'x',  'u',  's',  ' ',  '7'})});

    EXPECT_EQ(element_signature(elements), "33,33,221(0050f2,4),txpow:1400,wps:Nexus_7");
}

TEST_F(TestTaxonomy, test_taxonomy_the_last_element_of_an_id_gives_the_details)
{
    const std::vector<uint8_t> elements = join(
        {element(45, ht_body({0x11, 0x22})), element(45, ht_body({0x33, 0x44, 0x05}))});

    EXPECT_EQ(element_signature(elements), "45,45,htcap:4433,htagg:05,htmcs:00000000");
}

TEST_F(TestTaxonomy, test_taxonomy_a_damaged_element_ends_the_list)
{
    const std::vector<uint8_t> cut = join(
        {element(0, {}), element(1, {0x02}), std::vector<uint8_t> {0x03, 0x05, 0x01}});

    EXPECT_EQ(element_signature(cut), "0,1");
    EXPECT_EQ(element_signature({}), "");
}

TEST_F(TestTaxonomy, test_taxonomy_a_vendor_element_too_short_for_an_oui_is_listed_by_its_id)
{
    const std::vector<uint8_t> short_vendor = join({element(221, {0x00, 0x50}), element(7, {})});

    EXPECT_EQ(element_signature(short_vendor), "221,7");
}

TEST_F(TestTaxonomy, test_taxonomy_a_signature_needs_both_requests)
{
    const std::vector<uint8_t> probe = element(0, {});
    const std::vector<uint8_t> nothing;

    const Result<std::string> without_assoc = taxonomy::signature(ArrUByteView(probe), ArrUByteView(nothing));
    const Result<std::string> without_probe = taxonomy::signature(ArrUByteView(nothing), ArrUByteView(probe));

    ASSERT_FALSE(without_assoc.has_value());
    EXPECT_EQ(without_assoc.error().code, ErrorCode::invalid_argument);
    ASSERT_FALSE(without_probe.has_value());
    EXPECT_EQ(without_probe.error().code, ErrorCode::invalid_argument);
}

} // namespace test
