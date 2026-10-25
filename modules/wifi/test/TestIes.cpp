#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/wifi/ies.hpp>

#include "wifi_test_helpers.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::wifi;

class TestIes: public ::testing::Test
{
    protected:
        TestIes() { LoggerManager::stream(); }

        virtual ~TestIes() { LoggerManager::clear_loggers(); }

        static Result<std::vector<ies::Element>> walk(const std::vector<uint8_t> & bytes)
        {
            return ies::elements(ArrUByteView(bytes));
        }
};

TEST_F(TestIes, test_ies_elements)
{
    const std::vector<uint8_t> bytes = {0x00, 0x04, 'h', 'o', 'm', 'e', 0x01, 0x02, 0x82, 0x84, 0x03, 0x01, 0x06};

    const Result<std::vector<ies::Element>> elements = walk(bytes);
    const Result<std::vector<ies::Element>> empty = walk({});

    ASSERT_TRUE(elements.has_value());
    ASSERT_TRUE(empty.has_value());
    ASSERT_EQ(elements->size(), 3u);
    EXPECT_EQ((*elements)[0].id, ies::Id::ssid);
    EXPECT_EQ((*elements)[0].data.size(), 4u);
    EXPECT_EQ((*elements)[1].id, ies::Id::rates);
    EXPECT_EQ((*elements)[1].data[1], 0x84);
    EXPECT_EQ((*elements)[2].id, ies::Id::ds_parameter);
    EXPECT_EQ((*elements)[2].data[0], 0x06);
    EXPECT_EQ(empty->size(), 0u);
    ASSERT_TRUE(ies::find(*elements, ies::Id::ds_parameter).has_value());
    EXPECT_EQ(ies::find(*elements, ies::Id::ds_parameter)->data[0], 0x06);
    EXPECT_FALSE(ies::find(*elements, ies::Id::rsn).has_value());
}

TEST_F(TestIes, test_ies_truncated_elements_are_rejected_but_the_complete_ones_stay_readable)
{
    const std::vector<uint8_t> cut_rsn = {0x00, 0x04, 'h', 'o', 'm', 'e', 0x03, 0x01, 0x0b, 0x30, 0x14, 0x01};
    const std::vector<uint8_t> cut_header = {0x00, 0x04, 'h', 'o', 'm', 'e', 0x03};

    const std::vector<ies::Element> kept = ies::readable_elements(ArrUByteView(cut_rsn));
    const std::vector<ies::Element> kept_before_header = ies::readable_elements(ArrUByteView(cut_header));

    EXPECT_EQ(failure_of(walk({0x00, 0x04, 'h', 'o'})), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(walk({0x00, 0x01, 'h', 0x03})), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(walk(cut_rsn)), ErrorCode::invalid_argument);
    ASSERT_EQ(kept.size(), 2u);
    EXPECT_EQ(ies::ssid(kept), "home");
    EXPECT_EQ(ies::channel(kept), 11);
    EXPECT_EQ(kept_before_header.size(), 1u);
    EXPECT_EQ(ies::readable_elements(ArrUByteView(std::vector<uint8_t> {})).size(), 0u);
}

TEST_F(TestIes, test_ies_ssid_and_channel)
{
    const std::vector<uint8_t> named = {0x01, 0x01, 0x82, 0x00, 0x04, 'h', 'o', 'm', 'e'};
    const std::vector<uint8_t> hidden = {0x00, 0x00, 0x03, 0x01, 0x06};
    const std::vector<uint8_t> missing = {0x03, 0x01, 0x06};
    const std::vector<uint8_t> ds_parameter_first = {0x3d, 0x01, 0x24, 0x03, 0x01, 0x06};
    const std::vector<uint8_t> ht_operation = {0x00, 0x00, 0x3d, 0x01, 0x24};
    const std::vector<uint8_t> neither = {0x00, 0x00};

    EXPECT_EQ(ies::ssid(elements_of(named)), "home");
    EXPECT_EQ(ies::ssid(elements_of(hidden)), "");
    EXPECT_EQ(elements_of(hidden).size(), 2u);
    EXPECT_EQ(ies::ssid(elements_of(missing)), "");
    EXPECT_EQ(ies::channel(elements_of(hidden)), 6);
    EXPECT_EQ(ies::channel(elements_of(ds_parameter_first)), 6);
    EXPECT_EQ(ies::channel(elements_of(ht_operation)), 36);
    EXPECT_FALSE(ies::channel(elements_of(neither)).has_value());
}

TEST_F(TestIes, test_ies_cipher_and_akm_names)
{
    const Oui other = {0x00, 0x40, 0x96};

    EXPECT_EQ(ies::cipher_str({ies::ieee_oui, 1}), "wep40");
    EXPECT_EQ(ies::cipher_str({ies::ieee_oui, 2}), "tkip");
    EXPECT_EQ(ies::cipher_str({ies::ieee_oui, 4}), "ccmp");
    EXPECT_EQ(ies::cipher_str({ies::ieee_oui, 5}), "wep104");
    EXPECT_EQ(ies::cipher_str({ies::ieee_oui, 8}), "gcmp");
    EXPECT_EQ(ies::cipher_str({ies::ieee_oui, 9}), "gcmp256");
    EXPECT_EQ(ies::cipher_str({ies::ieee_oui, 10}), "ccmp256");
    EXPECT_EQ(ies::cipher_str({ies::microsoft_oui, 2}), "tkip");
    EXPECT_EQ(ies::cipher_str({ies::microsoft_oui, 8}), "unknown");
    EXPECT_EQ(ies::cipher_str({other, 4}), "unknown");

    EXPECT_EQ(ies::akm_str({ies::ieee_oui, 1}), "8021x");
    EXPECT_EQ(ies::akm_str({ies::ieee_oui, 2}), "psk");
    EXPECT_EQ(ies::akm_str({ies::ieee_oui, 3}), "ft_8021x");
    EXPECT_EQ(ies::akm_str({ies::ieee_oui, 4}), "ft_psk");
    EXPECT_EQ(ies::akm_str({ies::ieee_oui, 5}), "8021x_sha256");
    EXPECT_EQ(ies::akm_str({ies::ieee_oui, 6}), "psk_sha256");
    EXPECT_EQ(ies::akm_str({ies::ieee_oui, 8}), "sae");
    EXPECT_EQ(ies::akm_str({ies::ieee_oui, 9}), "ft_sae");
    EXPECT_EQ(ies::akm_str({ies::ieee_oui, 18}), "owe");
    EXPECT_EQ(ies::akm_str({ies::ieee_oui, 7}), "unknown");

    EXPECT_EQ(ies::akm_str({ies::microsoft_oui, 1}), "8021x");
    EXPECT_EQ(ies::akm_str({ies::microsoft_oui, 2}), "psk");
    EXPECT_EQ(ies::akm_str({other, 2}), "unknown");
}

TEST_F(TestIes, test_ies_rates)
{
    const std::vector<uint8_t> bytes = {0x01,
                                        0x08,
                                        0x82,
                                        0x84,
                                        0x8b,
                                        0x96,
                                        0x0c,
                                        0x12,
                                        0x18,
                                        0x24,
                                        0x32,
                                        0x07,
                                        0x30,
                                        0x48,
                                        0x60,
                                        0x6c,
                                        0xff,
                                        0xf9,
                                        0xf8};

    const std::vector<ies::Rate> rates = ies::rates(elements_of(bytes));

    ASSERT_EQ(rates.size(), 12u);
    EXPECT_EQ(rates[0].kbps, 1000u);
    EXPECT_TRUE(rates[0].basic);
    EXPECT_EQ(rates[3].kbps, 11000u);
    EXPECT_TRUE(rates[3].basic);
    EXPECT_EQ(rates[4].kbps, 6000u);
    EXPECT_FALSE(rates[4].basic);
    EXPECT_EQ(rates.back().kbps, 54000u);
    EXPECT_FALSE(rates.back().basic);
}

TEST_F(TestIes, test_ies_rsn)
{
    const std::vector<uint8_t> body = {0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f,
                                       0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x0c, 0x00};

    const Result<ies::Rsn> rsn = ies::parse_rsn(ArrUByteView(body.data(), body.size()));

    ASSERT_TRUE(rsn.has_value());
    EXPECT_EQ(rsn->version, 1u);
    EXPECT_EQ(rsn->group, (ies::Suite {.oui = ies::ieee_oui, .type = 4}));
    ASSERT_EQ(rsn->pairwise.size(), 1u);
    EXPECT_EQ(ies::cipher_str(rsn->pairwise[0]), "ccmp");
    ASSERT_EQ(rsn->akms.size(), 1u);
    EXPECT_EQ(ies::akm_str(rsn->akms[0]), "psk");
    EXPECT_EQ(rsn->capabilities, 0x000cu);
}

TEST_F(TestIes, test_ies_rsn_stops_where_the_body_does)
{
    const std::vector<uint8_t> group_only = {0x01, 0x00, 0x00, 0x0f, 0xac, 0x02};
    const std::vector<uint8_t> no_group = {0x01, 0x00, 0x00, 0x0f};
    const std::vector<uint8_t> pairwise_overrun =
        {0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x05, 0x00, 0x00, 0x0f, 0xac, 0x04};
    const std::vector<uint8_t> count_cut = {0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01};

    const Result<ies::Rsn> parsed = ies::parse_rsn(ArrUByteView(group_only.data(), group_only.size()));

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(ies::cipher_str(parsed->group), "tkip");
    EXPECT_EQ(parsed->pairwise.size(), 0u);
    EXPECT_EQ(parsed->akms.size(), 0u);
    EXPECT_FALSE(parsed->capabilities.has_value());
    EXPECT_EQ(failure_of(ies::parse_rsn(ArrUByteView(no_group))), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(ies::parse_rsn(ArrUByteView(pairwise_overrun))), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(ies::parse_rsn(ArrUByteView(count_cut))), ErrorCode::invalid_argument);
}

TEST_F(TestIes, test_ies_wpa_vendor_element)
{
    const std::vector<uint8_t> bytes = {0xdd, 0x16, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00, 0x00, 0x50, 0xf2,
                                        0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50,
                                        0xf2, 0x02, 0xdd, 0x05, 0x00, 0x10, 0x18, 0x02, 0x00};
    const std::vector<ies::Element> elements = elements_of(bytes);

    const std::optional<ies::Vendor> wpa = ies::find_vendor(elements, ies::microsoft_oui, 1);
    const std::optional<ies::Vendor> broadcom = ies::find_vendor(elements, Oui {0x00, 0x10, 0x18}, 2);

    ASSERT_TRUE(wpa.has_value());
    EXPECT_EQ(wpa->data.size(), 18u);
    EXPECT_FALSE(ies::find_vendor(elements, ies::microsoft_oui, 4).has_value());
    EXPECT_FALSE(ies::find_vendor(elements, ies::ieee_oui, 1).has_value());
    ASSERT_TRUE(broadcom.has_value());
    EXPECT_EQ(broadcom->data.size(), 1u);
    const Result<ies::Rsn> rsn = ies::parse_rsn(wpa->data);
    ASSERT_TRUE(rsn.has_value());
    EXPECT_EQ(ies::cipher_str(rsn->group), "tkip");
    ASSERT_EQ(rsn->akms.size(), 1u);
    EXPECT_EQ(ies::akm_str(rsn->akms[0]), "psk");
}

TEST_F(TestIes, test_ies_wps)
{
    const std::vector<uint8_t> attributes = {0x10, 0x4a, 0x00, 0x01, 0x10, 0x10, 0x44, 0x00, 0x01, 0x02, 0x10, 0x57,
                                             0x00, 0x01, 0x01, 0x10, 0x21, 0x00, 0x03, 'a',  'c',  'm',  0x10, 0x23,
                                             0x00, 0x02, 'x',  '1',  0x10, 0x11, 0x00, 0x03, 'f',  'o',  'o'};
    const std::vector<uint8_t> truncated = {0x10, 0x21, 0x00, 0x09, 'a'};

    const Result<ies::Wps> wps = ies::parse_wps(ArrUByteView(attributes.data(), attributes.size()));

    ASSERT_TRUE(wps.has_value());
    EXPECT_EQ(wps->version, 0x10u);
    EXPECT_EQ(wps->setup_state, 2u);
    EXPECT_TRUE(wps->ap_setup_locked);
    EXPECT_EQ(wps->manufacturer, "acm");
    EXPECT_EQ(wps->model_name, "x1");
    EXPECT_EQ(wps->model_number, "");
    EXPECT_EQ(wps->device_name, "foo");
    EXPECT_EQ(failure_of(ies::parse_wps(ArrUByteView(truncated))), ErrorCode::invalid_argument);
}

TEST_F(TestIes, test_ies_lookup_rsn_wpa_and_wps)
{
    const std::vector<uint8_t> bytes = {0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f,
                                        0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x0c, 0x00, 0xdd, 0x16,
                                        0x00, 0x50, 0xf2, 0x01, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x00,
                                        0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02, 0xdd, 0x09,
                                        0x00, 0x50, 0xf2, 0x04, 0x10, 0x4a, 0x00, 0x01, 0x10};
    const std::vector<uint8_t> bare = {0x00, 0x04, 'h', 'o', 'm', 'e'};
    const std::vector<uint8_t> cut_rsn = {0x30, 0x03, 0x01, 0x00, 0x00};
    const std::vector<ies::Element> elements = elements_of(bytes);
    const std::vector<ies::Element> bare_elements = elements_of(bare);
    const std::vector<ies::Element> cut_elements = elements_of(cut_rsn);

    const Result<std::optional<ies::Rsn>> rsn = ies::rsn(elements);
    const Result<std::optional<ies::Rsn>> wpa = ies::wpa(elements);
    const Result<std::optional<ies::Wps>> wps = ies::wps(elements);

    ASSERT_TRUE(rsn.has_value());
    ASSERT_TRUE(rsn->has_value());
    EXPECT_EQ(ies::cipher_str((*rsn)->group), "ccmp");
    ASSERT_TRUE(wpa.has_value());
    ASSERT_TRUE(wpa->has_value());
    EXPECT_EQ(ies::cipher_str((*wpa)->group), "tkip");
    ASSERT_TRUE(wps.has_value());
    ASSERT_TRUE(wps->has_value());
    EXPECT_EQ((*wps)->version, 0x10u);
    EXPECT_EQ(failure_of(ies::rsn(cut_elements)), ErrorCode::invalid_argument);

    const Result<std::optional<ies::Rsn>> no_rsn = ies::rsn(bare_elements);
    const Result<std::optional<ies::Rsn>> no_wpa = ies::wpa(bare_elements);
    const Result<std::optional<ies::Wps>> no_wps = ies::wps(bare_elements);

    ASSERT_TRUE(no_rsn.has_value());
    ASSERT_TRUE(no_wpa.has_value());
    ASSERT_TRUE(no_wps.has_value());
    EXPECT_FALSE(no_rsn->has_value());
    EXPECT_FALSE(no_wpa->has_value());
    EXPECT_FALSE(no_wps->has_value());
}

} // namespace test
