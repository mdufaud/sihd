#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/wifi/capabilities.hpp>

#include "wifi_test_helpers.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::wifi;

class TestCapabilities: public ::testing::Test
{
    protected:
        TestCapabilities() { LoggerManager::stream(); }

        virtual ~TestCapabilities() { LoggerManager::clear_loggers(); }
};

TEST_F(TestCapabilities, test_capabilities_ht_of_a_real_probe_request)
{
    // the HT capabilities of the probe request in tcpdump's ieee802.11_exthdr.pcap: a 2x2 radio
    std::vector<uint8_t> bytes = {0x2d, 0x1a, 0xce, 0x11, 0x1b, 0xff, 0xff};
    bytes.insert(bytes.end(), 10, 0x00);
    bytes.push_back(0x01);
    bytes.insert(bytes.end(), 10, 0x00);
    const std::vector<ies::Element> elements = elements_of(bytes);

    const Result<std::optional<ies::HtCapabilities>> result = ies::ht_capabilities(elements);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->has_value());
    const ies::HtCapabilities & ht = **result;
    EXPECT_EQ(ht.info, 0x11ceu);
    EXPECT_FALSE(ht.ldpc);
    EXPECT_TRUE(ht.width_40mhz);
    EXPECT_EQ(ht.spatial_multiplexing_power_save, 3u);
    EXPECT_FALSE(ht.greenfield);
    EXPECT_FALSE(ht.short_gi_20mhz);
    EXPECT_TRUE(ht.short_gi_40mhz);
    EXPECT_TRUE(ht.tx_stbc);
    EXPECT_EQ(ht.rx_stbc_streams, 1u);
    EXPECT_FALSE(ht.max_amsdu_7935);
    EXPECT_TRUE(ht.dsss_cck_40mhz);

    EXPECT_FALSE(ht.forty_mhz_intolerant);
    EXPECT_EQ(ht.ampdu_length_exponent, 3u);
    EXPECT_EQ(ht.ampdu_min_spacing, 6u);
    EXPECT_EQ(ht.rx_spatial_streams, 2u);
    EXPECT_EQ(ht.rx_highest_rate_mbps, 0u);
}

TEST_F(TestCapabilities, test_capabilities_ht_flags_the_real_probe_request_leaves_off)
{
    std::vector<uint8_t> bytes = {0x2d, 0x1a, 0x35, 0x4b, 0x1d, 0xff, 0xff, 0xff, 0x00};
    bytes.insert(bytes.end(), 6, 0x00);
    bytes.insert(bytes.end(), {0x96, 0x00});
    bytes.insert(bytes.end(), 11, 0x00);

    const Result<std::optional<ies::HtCapabilities>> result = ies::ht_capabilities(elements_of(bytes));

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->has_value());
    const ies::HtCapabilities & ht = **result;

    EXPECT_TRUE(ht.ldpc);
    EXPECT_FALSE(ht.width_40mhz);
    EXPECT_EQ(ht.spatial_multiplexing_power_save, 1u);
    EXPECT_TRUE(ht.greenfield);
    EXPECT_TRUE(ht.short_gi_20mhz);
    EXPECT_FALSE(ht.short_gi_40mhz);
    EXPECT_FALSE(ht.tx_stbc);
    EXPECT_EQ(ht.rx_stbc_streams, 3u);

    EXPECT_TRUE(ht.max_amsdu_7935);
    EXPECT_FALSE(ht.dsss_cck_40mhz);
    EXPECT_TRUE(ht.forty_mhz_intolerant);

    EXPECT_EQ(ht.ampdu_length_exponent, 1u);
    EXPECT_EQ(ht.ampdu_min_spacing, 7u);
    EXPECT_EQ(ht.rx_spatial_streams, 3u);
    EXPECT_EQ(ht.rx_highest_rate_mbps, 150u);
}

TEST_F(TestCapabilities, test_capabilities_ht_absent_or_cut)
{
    const std::vector<uint8_t> other = {0x00, 0x00};
    const std::vector<uint8_t> cut = {0x2d, 0x05, 0xce, 0x11, 0x1b, 0xff, 0xff};

    const Result<std::optional<ies::HtCapabilities>> absent = ies::ht_capabilities(elements_of(other));
    const Result<std::optional<ies::HtCapabilities>> truncated = ies::ht_capabilities(elements_of(cut));

    ASSERT_TRUE(absent.has_value());
    EXPECT_FALSE(absent->has_value());
    ASSERT_FALSE(truncated.has_value());
    EXPECT_EQ(truncated.error().code, ErrorCode::invalid_argument);
}

TEST_F(TestCapabilities, test_capabilities_vht_of_a_known_phone)
{
    // vhtcap 33d071b0, rx and tx map fffa, highest rate 360 Mb/s: the signature of a Pixel in Google's wifi taxonomy
    const std::vector<uint8_t> bytes =
        {0xbf, 0x0c, 0xb0, 0x71, 0xd0, 0x33, 0xfa, 0xff, 0x68, 0x01, 0xfa, 0xff, 0x68, 0x01};
    const std::vector<uint8_t> wide =
        {0xbf, 0x0c, 0x0a, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x00, 0x00, 0xfc, 0xff, 0x00, 0x00};

    const Result<std::optional<ies::VhtCapabilities>> phone = ies::vht_capabilities(elements_of(bytes));
    const Result<std::optional<ies::VhtCapabilities>> router = ies::vht_capabilities(elements_of(wide));

    ASSERT_TRUE(phone.has_value());
    ASSERT_TRUE(phone->has_value());
    ASSERT_TRUE(router.has_value());
    ASSERT_TRUE(router->has_value());
    EXPECT_EQ((*phone)->info, 0x33d071b0u);
    EXPECT_EQ((*phone)->max_mpdu_length, 3895u);
    EXPECT_FALSE((*phone)->width_160mhz);
    EXPECT_TRUE((*phone)->rx_ldpc);
    EXPECT_TRUE((*phone)->short_gi_80mhz);
    EXPECT_FALSE((*phone)->short_gi_160mhz);
    EXPECT_TRUE((*phone)->tx_stbc);
    EXPECT_EQ((*phone)->rx_stbc_streams, 1u);
    EXPECT_FALSE((*phone)->su_beamformer);
    EXPECT_TRUE((*phone)->su_beamformee);

    EXPECT_FALSE((*phone)->mu_beamformer);
    EXPECT_TRUE((*phone)->mu_beamformee);
    EXPECT_EQ((*phone)->ampdu_length_exponent, 7u);
    EXPECT_EQ((*phone)->rx_mcs_map, 0xfffau);
    EXPECT_EQ((*phone)->rx_spatial_streams, 2u);
    EXPECT_EQ((*phone)->tx_spatial_streams, 2u);
    EXPECT_EQ((*router)->max_mpdu_length, 11454u);
    EXPECT_TRUE((*router)->width_160mhz);
    EXPECT_TRUE((*router)->width_80plus80mhz);
    EXPECT_EQ((*router)->rx_spatial_streams, 1u);
}

TEST_F(TestCapabilities, test_capabilities_vht_flags_the_phone_leaves_off)
{
    const std::vector<uint8_t> bytes =
        {0xbf, 0x0c, 0x45, 0x08, 0x88, 0x01, 0xff, 0xff, 0x00, 0x00, 0xfe, 0xff, 0x00, 0x00};
    const std::vector<uint8_t> reserved_values =
        {0xbf, 0x0c, 0x0f, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00};

    const Result<std::optional<ies::VhtCapabilities>> flags = ies::vht_capabilities(elements_of(bytes));
    const Result<std::optional<ies::VhtCapabilities>> reserved_decoded = ies::vht_capabilities(
        elements_of(reserved_values));

    ASSERT_TRUE(flags.has_value());
    ASSERT_TRUE(flags->has_value());
    ASSERT_TRUE(reserved_decoded.has_value());
    ASSERT_TRUE(reserved_decoded->has_value());
    EXPECT_EQ((*flags)->max_mpdu_length, 7991u);
    EXPECT_TRUE((*flags)->width_160mhz);
    EXPECT_FALSE((*flags)->width_80plus80mhz);
    EXPECT_FALSE((*flags)->rx_ldpc);
    EXPECT_FALSE((*flags)->short_gi_80mhz);
    EXPECT_TRUE((*flags)->short_gi_160mhz);
    EXPECT_FALSE((*flags)->tx_stbc);
    EXPECT_EQ((*flags)->rx_stbc_streams, 0u);

    EXPECT_TRUE((*flags)->su_beamformer);
    EXPECT_FALSE((*flags)->su_beamformee);
    EXPECT_TRUE((*flags)->mu_beamformer);
    EXPECT_FALSE((*flags)->mu_beamformee);

    EXPECT_EQ((*flags)->ampdu_length_exponent, 3u);
    EXPECT_EQ((*flags)->rx_spatial_streams, 0u);
    EXPECT_EQ((*flags)->tx_spatial_streams, 1u);

    EXPECT_EQ((*reserved_decoded)->max_mpdu_length, 0u);
    EXPECT_FALSE((*reserved_decoded)->width_160mhz);
    EXPECT_FALSE((*reserved_decoded)->width_80plus80mhz);
}

TEST_F(TestCapabilities, test_capabilities_vht_cut)
{
    const std::vector<uint8_t> cut = {0xbf, 0x04, 0xb0, 0x71, 0xd0, 0x33};

    const Result<std::optional<ies::VhtCapabilities>> truncated = ies::vht_capabilities(elements_of(cut));

    ASSERT_FALSE(truncated.has_value());
    EXPECT_EQ(truncated.error().code, ErrorCode::invalid_argument);
}

TEST_F(TestCapabilities, test_capabilities_country)
{
    const std::vector<uint8_t> united_states = {0x07, 0x09, 'U', 'S', ' ', 0x01, 0x0b, 0x1e, 0x24, 0x04, 0x17};
    const std::vector<uint8_t> france_indoor = {0x07, 0x09, 'F', 'R', 'I', 0x01, 0x0d, 0x14, 0xc9, 0x01, 0x02};
    const std::vector<uint8_t> cut = {0x07, 0x02, 'U', 'S'};

    const Result<std::optional<ies::Country>> us_country = ies::country(elements_of(united_states));
    const Result<std::optional<ies::Country>> fr_country = ies::country(elements_of(france_indoor));
    const Result<std::optional<ies::Country>> truncated = ies::country(elements_of(cut));

    ASSERT_TRUE(us_country.has_value());
    ASSERT_TRUE(us_country->has_value());
    ASSERT_TRUE(fr_country.has_value());
    ASSERT_TRUE(fr_country->has_value());
    EXPECT_EQ((*us_country)->code, "US");
    EXPECT_EQ((*us_country)->environment, ' ');
    ASSERT_EQ((*us_country)->channels.size(), 2u);
    EXPECT_EQ((*us_country)->channels[0].first, 1u);
    EXPECT_EQ((*us_country)->channels[0].count, 11u);
    EXPECT_EQ((*us_country)->channels[0].max_power_dbm, 30);
    EXPECT_EQ((*us_country)->channels[1].first, 36u);
    EXPECT_EQ((*us_country)->channels[1].count, 4u);
    EXPECT_EQ((*fr_country)->code, "FR");
    EXPECT_EQ((*fr_country)->environment, 'I');

    ASSERT_EQ((*fr_country)->channels.size(), 1u);
    EXPECT_EQ((*fr_country)->channels[0].count, 13u);
    ASSERT_FALSE(truncated.has_value());
    EXPECT_EQ(truncated.error().code, ErrorCode::invalid_argument);
}

TEST_F(TestCapabilities, test_capabilities_tim)
{
    const std::vector<uint8_t> multicast = {0x05, 0x04, 0x00, 0x02, 0x01, 0x00};
    const std::vector<uint8_t> unicast_only = {0x05, 0x05, 0x01, 0x03, 0x00, 0xff, 0xff};
    const std::vector<uint8_t> cut = {0x05, 0x02, 0x00, 0x02};

    const Result<std::optional<ies::Tim>> first = ies::tim(elements_of(multicast));
    const Result<std::optional<ies::Tim>> second = ies::tim(elements_of(unicast_only));

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(first->has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(second->has_value());
    EXPECT_EQ((*first)->dtim_count, 0u);
    EXPECT_EQ((*first)->dtim_period, 2u);
    EXPECT_TRUE((*first)->multicast_buffered);
    EXPECT_EQ((*second)->dtim_count, 1u);
    EXPECT_EQ((*second)->dtim_period, 3u);
    EXPECT_FALSE((*second)->multicast_buffered);
    EXPECT_EQ(failure_of(ies::tim(elements_of(cut))), ErrorCode::invalid_argument);
}

TEST_F(TestCapabilities, test_capabilities_standards)
{
    const std::vector<uint8_t> wifi4 = {0x2d, 0x00};
    const std::vector<uint8_t> wifi6 = {0x2d, 0x00, 0xbf, 0x00, 0xff, 0x01, 0x23};
    const std::vector<uint8_t> wifi7 = {0xff, 0x01, 0x23, 0xff, 0x01, 0x6c};
    const std::vector<uint8_t> other_extension = {0x00, 0x00, 0xff, 0x01, 0x24};

    const ies::Standards wifi4_standards = ies::standards(elements_of(wifi4));
    const ies::Standards wifi6_standards = ies::standards(elements_of(wifi6));
    const ies::Standards wifi7_standards = ies::standards(elements_of(wifi7));
    const ies::Standards other_extension_standards = ies::standards(elements_of(other_extension));

    EXPECT_TRUE(wifi4_standards.ht);
    EXPECT_FALSE(wifi4_standards.vht);
    EXPECT_FALSE(wifi4_standards.he);
    EXPECT_TRUE(wifi6_standards.ht);
    EXPECT_TRUE(wifi6_standards.vht);
    EXPECT_TRUE(wifi6_standards.he);
    EXPECT_FALSE(wifi6_standards.eht);
    EXPECT_TRUE(wifi7_standards.he);
    EXPECT_TRUE(wifi7_standards.eht);
    EXPECT_FALSE(wifi7_standards.ht);
    EXPECT_FALSE(other_extension_standards.ht);
    EXPECT_FALSE(other_extension_standards.vht);
    EXPECT_FALSE(other_extension_standards.he);
    EXPECT_FALSE(other_extension_standards.eht);
}

TEST_F(TestCapabilities, test_capabilities_vendors)
{
    const std::vector<uint8_t> bytes = {0xdd, 0x07, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00, 0x00, 0xdd, 0x02,
                                        0x00, 0x50, 0xdd, 0x05, 0x00, 0x10, 0x18, 0x02, 0x7f, 0x00, 0x00};

    const std::vector<ies::Vendor> vendors = ies::vendors(elements_of(bytes));

    ASSERT_EQ(vendors.size(), 2u);
    EXPECT_EQ(vendors[0].oui, ies::microsoft_oui);
    EXPECT_EQ(vendors[0].type, 1u);
    EXPECT_EQ(vendors[0].data.size(), 3u);
    EXPECT_EQ(vendors[1].oui, (Oui {0x00, 0x10, 0x18}));
    EXPECT_EQ(vendors[1].type, 2u);
    EXPECT_EQ(vendors[1].data[0], 0x7f);
}

} // namespace test
