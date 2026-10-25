#include <cstdint>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>
#include <sihd/wifi/data.hpp>
#include <sihd/wifi/dissect.hpp>
#include <sihd/wifi/ies.hpp>
#include <sihd/wifi/management.hpp>

#include "wifi_test_helpers.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::wifi;

namespace
{

// tcpdump 4.99.6 ieee802.11_exthdr.pcap packet 1: probe request, extended radiotap presence, fcs at end
constexpr std::string_view real_probe_request =
    "000059006f480080f70177100000000068d698000000000010026c09a000eaaa01000000000002006c09a000031b68d6"
    "98000000000001000140334080aa009fff019cffaa000000000000000000000000000000000000000040000000ffffff"
    "ffffff90a4dec04611ffffffffffff100000046f6d7573010802040b160c12182432043048606c0301012d1ace111bff"
    "ff00000000000000000000010000000000000000000007ae1c88";

// tcpdump 4.99.6 ieee802.11_exthdr.pcap packet 2: acknowledgment, fcs at end
constexpr std::string_view
    real_ack = "000059006f480080f7017710000000006ae098000000000010026c09a000edaa00000000000002006c09a000031b6ae0"
               "9800000000003d040043364380aa009fff019cff670000000000000000000000000000000000000000d400000090a4de"
               "c0460a2731633c";

// same file, packet 3: probe response, radiotap without channel nor signal
constexpr std::string_view real_probe_response =
    "0000530045840280777f761000000000ddd998000000000002aa1b000000000000026c09a000031bfc06000000000000"
    "0000000000000043364380aa009fff019cffe10000000000000000000000000000000050003a0190a4dec0461190a4de"
    "c0460a90a4dec0460ac06f00000000000000006400010400046f6d7573010882848b960c1218240301012a0104320430"
    "48606c2d1a0c001bffff0000000000000000000001000000000000000000003d16010011000000000000000000000000"
    "00000000000000dd180050f2020101000003a4000027a4000042435e0062322f00";

// tcpdump 4.99.6 tests/ieee802.11_rx-stbc.pcap packet 1: protected QoS data, radiotap with MCS
constexpr std::string_view
    real_qos_data = "000025002b480800641c00000000000010009e098004cd010000272507000000000000000088422c0068a3c40346da20"
                    "7c8f503f3a207c8f503f3a200100001100002000000000aaaa03000000080045000054bcd2000040013a7cc0a80106c0"
                    "a8010400003ba3100d000fe3998351000000008b82030000000000101112131415161718191a1b1c1d1e1f2021222324"
                    "25262728292a2b2c2d2e2f3031323334353637b1b6bfc1ebc4a320c6a02c87";

// tcpdump 4.99.6 ieee802.11_exthdr.pcap packet 25: null data, MCS 2, 20 MHz, long GI (19.5 Mb/s)
constexpr std::string_view real_null_data_mcs_2 =
    "00005d006b480880fb01771000000000a9c9cb000000000010006c098004eaaa0100000007000200000700026c098004"
    "031ba9c9cb00000000001d000140354080aa009fff019cff790000000000000000000000000000000000000000480130"
    "0090a4dec0460a90a4dec0461190a4dec0460ad001ad7f46ff";

// tcpdump 4.99.6 ieee802.11_exthdr.pcap packet 26: null data, MCS 11, 20 MHz, long GI (52 Mb/s)
constexpr std::string_view real_null_data_mcs_11 =
    "00005d006b480880fb01771000000000c74dcd000000000010006c098004ebaa0100000007000b000007000b6c098004"
    "031bc74dcd00000000001e000141374180aa009fff019cff79000000000000000000000000000000000000000048112c"
    "0090a4dec0460a90a4dec0461190a4dec0460ae0015330676d";

} // namespace

class TestDissect: public ::testing::Test
{
    protected:
        TestDissect() { LoggerManager::stream(); }

        virtual ~TestDissect() { LoggerManager::clear_loggers(); }

        static constexpr MacAddress mac_a = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
        static constexpr MacAddress broadcast = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        static constexpr MacAddress mac_station = {0x90, 0xa4, 0xde, 0xc0, 0x46, 0x11};
        static constexpr MacAddress mac_ap = {0x90, 0xa4, 0xde, 0xc0, 0x46, 0x0a};

        static std::vector<uint8_t> from_hex(std::string_view hex)
        {
            Result<std::vector<uint8_t>> bytes = str::from_hex(hex);
            if (bytes.has_value() == false)
            {
                ADD_FAILURE() << bytes.error().message;
                return {};
            }
            return std::move(*bytes);
        }

        template <typename T>
        static bool graceful(const Result<T> & result)
        {
            return result.has_value() || result.error().code == ErrorCode::invalid_argument
                   || result.error().code == ErrorCode::not_supported;
        }

        static std::vector<uint8_t> header_of(uint8_t byte0, uint8_t byte1)
        {
            std::vector<uint8_t> bytes = {byte0, byte1, 0x00, 0x00};
            bytes.insert(bytes.end(), broadcast.begin(), broadcast.end());
            bytes.insert(bytes.end(), mac_a.begin(), mac_a.end());
            bytes.insert(bytes.end(), broadcast.begin(), broadcast.end());
            bytes.insert(bytes.end(), {0x00, 0x00});
            return bytes;
        }
};

TEST_F(TestDissect, test_dissect_radiotap_skips_data_padding)
{
    std::vector<uint8_t> bytes = {0x00, 0x00, 0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20};
    const std::vector<uint8_t> frame = header_of(0x88, 0x01);
    bytes.insert(bytes.end(), frame.begin(), frame.end());
    bytes.insert(bytes.end(), {0x00, 0x00, 0xee, 0xee, 0x0a, 0x0b, 0x0c});

    const Result<Packet> packet = dissect_radiotap(ArrUByteView(bytes));

    ASSERT_TRUE(packet.has_value());
    ASSERT_TRUE(packet->radiotap.has_value());
    EXPECT_TRUE(packet->radiotap->data_padding);
    EXPECT_EQ(packet->frame.header_size, 26u);
    ASSERT_EQ(packet->frame.body.size(), 3u);
    EXPECT_EQ(packet->frame.body[0], 0x0a);
}

TEST_F(TestDissect, test_dissect_radiotap_drops_the_fcs)
{
    std::vector<uint8_t> bytes = {0x00, 0x00, 0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0x50};
    bytes.insert(bytes.end(), {0xd4, 0x00, 0x00, 0x00});
    bytes.insert(bytes.end(), mac_a.begin(), mac_a.end());
    bytes.insert(bytes.end(), {0xaa, 0xbb, 0xcc, 0xdd});

    const Result<Packet> packet = dissect_radiotap(ArrUByteView(bytes));

    ASSERT_TRUE(packet.has_value());
    ASSERT_TRUE(packet->radiotap.has_value());
    EXPECT_TRUE(packet->radiotap->bad_fcs);
    EXPECT_EQ(packet->frame.type, Frame::Type::control);
    EXPECT_EQ(packet->frame.header_size, 10u);
    EXPECT_EQ(packet->frame.body.size(), 0u);
}

TEST_F(TestDissect, test_dissect_80211_has_no_radiotap)
{
    const std::vector<uint8_t> bytes = header_of(0x80, 0x00);

    const Result<Packet> packet = dissect_80211(ArrUByteView(bytes));

    ASSERT_TRUE(packet.has_value());
    EXPECT_FALSE(packet->radiotap.has_value());
    EXPECT_EQ(packet->frame.subtype, 8u);
    EXPECT_EQ(packet->frame.addr1, broadcast);
}

TEST_F(TestDissect, test_dissect_reports_the_layer_that_fails)
{
    const std::vector<uint8_t> truncated_radiotap = {0x00, 0x00, 0x08};
    const std::vector<uint8_t> truncated_frame = {0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00};
    const std::vector<uint8_t> empty;

    const Result<Packet> radiotap_failure = dissect_radiotap(ArrUByteView(truncated_radiotap));
    const Result<Packet> frame_failure = dissect_radiotap(ArrUByteView(truncated_frame));
    const Result<Packet> bare_failure = dissect_80211(ArrUByteView(empty));

    ASSERT_FALSE(radiotap_failure.has_value());
    ASSERT_FALSE(frame_failure.has_value());
    ASSERT_FALSE(bare_failure.has_value());
    EXPECT_EQ(radiotap_failure.error().message, "radiotap header needs 8 bytes");
    EXPECT_EQ(frame_failure.error().message, "802.11 frame needs 10 bytes");
    EXPECT_EQ(bare_failure.error().message, "802.11 frame needs 10 bytes");
}

TEST_F(TestDissect, test_dissect_real_probe_request_with_extended_presence)
{
    const std::vector<uint8_t> bytes = from_hex(real_probe_request);

    const Result<Packet> packet = dissect_radiotap(ArrUByteView(bytes));

    ASSERT_TRUE(packet.has_value());
    ASSERT_TRUE(packet->radiotap.has_value());
    EXPECT_EQ(packet->radiotap->header_size, 89u);
    EXPECT_EQ(packet->radiotap->tsft_us, 10016360u);
    EXPECT_EQ(packet->radiotap->rate_kbps, 1000u);
    EXPECT_EQ(packet->radiotap->frequency_mhz, 2412u);
    EXPECT_EQ(packet->radiotap->signal_dbm, -22);
    EXPECT_EQ(packet->radiotap->noise_dbm, -86);
    EXPECT_EQ(packet->radiotap->antenna, 1u);
    EXPECT_TRUE(packet->radiotap->fcs_at_end);
    EXPECT_EQ(packet->frame.type, Frame::Type::management);
    EXPECT_EQ(packet->frame.subtype, 4u);
    EXPECT_EQ(packet->frame.addr1, broadcast);
    EXPECT_EQ(packet->frame.addr2, mac_station);

    EXPECT_EQ(packet->frame.addr3, broadcast);
    EXPECT_EQ(packet->frame.body.size(), 53u);

    const Result<std::vector<ies::Element>> elements = ies::elements(packet->frame.body);

    ASSERT_TRUE(elements.has_value());
    EXPECT_EQ(ies::ssid(*elements), "omus");
    EXPECT_EQ(ies::channel(*elements), 1);
    ASSERT_EQ(ies::rates(*elements).size(), 12u);
    EXPECT_EQ(ies::rates(*elements).front().kbps, 1000u);
    EXPECT_EQ(ies::rates(*elements).back().kbps, 54000u);
}

TEST_F(TestDissect, test_dissect_real_ack_keeps_nothing_after_its_header)
{
    const std::vector<uint8_t> bytes = from_hex(real_ack);

    const Result<Packet> packet = dissect_radiotap(ArrUByteView(bytes));

    ASSERT_TRUE(packet.has_value());
    ASSERT_TRUE(packet->radiotap.has_value());
    EXPECT_EQ(packet->radiotap->signal_dbm, -19);
    EXPECT_EQ(packet->radiotap->antenna, 0u);
    EXPECT_TRUE(packet->radiotap->fcs_at_end);
    EXPECT_EQ(packet->frame.type, Frame::Type::control);
    EXPECT_EQ(packet->frame.subtype, 13u);
    EXPECT_EQ(packet->frame.header_size, 10u);
    EXPECT_EQ(packet->frame.addr1, mac_ap);
    EXPECT_FALSE(packet->frame.addr2.has_value());
    EXPECT_EQ(packet->frame.body.size(), 0u);
}

TEST_F(TestDissect, test_dissect_real_probe_response_without_channel_nor_signal)
{
    const std::vector<uint8_t> bytes = from_hex(real_probe_response);

    const Result<Packet> packet = dissect_radiotap(ArrUByteView(bytes));

    ASSERT_TRUE(packet.has_value());
    ASSERT_TRUE(packet->radiotap.has_value());
    EXPECT_EQ(packet->radiotap->tsft_us, 10017245u);
    EXPECT_EQ(packet->radiotap->noise_dbm, -86);
    EXPECT_FALSE(packet->radiotap->frequency_mhz.has_value());
    EXPECT_FALSE(packet->radiotap->signal_dbm.has_value());
    EXPECT_EQ(packet->frame.subtype, 5u);
    EXPECT_EQ(packet->frame.addr1, mac_station);
    EXPECT_EQ(packet->frame.addr2, mac_ap);
    EXPECT_EQ(packet->frame.addr3, mac_ap);

    const Result<Beacon> response = parse_probe_response(packet->frame.body);

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->interval_tu, 100u);
    EXPECT_EQ(response->capability, 0x0401u);
    EXPECT_TRUE(response->ess);
    EXPECT_FALSE(response->privacy);

    const Result<std::vector<ies::Element>> elements = ies::elements(response->information_elements);

    ASSERT_TRUE(elements.has_value());
    EXPECT_EQ(ies::ssid(*elements), "omus");
    EXPECT_EQ(ies::channel(*elements), 1);
    EXPECT_EQ(ies::rates(*elements).size(), 12u);

    const Result<std::optional<ies::Rsn>> rsn = ies::rsn(*elements);
    const Result<std::optional<ies::Rsn>> wpa = ies::wpa(*elements);

    ASSERT_TRUE(rsn.has_value());
    ASSERT_TRUE(wpa.has_value());
    EXPECT_FALSE(rsn->has_value());
    EXPECT_FALSE(wpa->has_value());
}

TEST_F(TestDissect, test_dissect_real_protected_qos_data_with_mcs)
{
    const std::vector<uint8_t> bytes = from_hex(real_qos_data);

    const Result<Packet> packet = dissect_radiotap(ArrUByteView(bytes));

    ASSERT_TRUE(packet.has_value());
    ASSERT_TRUE(packet->radiotap.has_value());
    EXPECT_EQ(packet->radiotap->header_size, 37u);
    EXPECT_EQ(packet->radiotap->tsft_us, 7268u);
    EXPECT_EQ(packet->radiotap->frequency_mhz, 2462u);
    EXPECT_EQ(packet->radiotap->signal_dbm, -51);
    EXPECT_EQ(packet->radiotap->antenna, 1u);
    EXPECT_FALSE(packet->radiotap->rate_kbps.has_value());
    ASSERT_TRUE(packet->radiotap->mcs.has_value());
    EXPECT_EQ(packet->radiotap->mcs->index, 7u);
    EXPECT_EQ(packet->radiotap->mcs->bandwidth_mhz, 40u);
    EXPECT_TRUE(packet->radiotap->mcs->short_gi);
    EXPECT_EQ(packet->radiotap->mcs->rate_kbps, 150000u);

    EXPECT_EQ(packet->frame.type, Frame::Type::data);
    EXPECT_EQ(packet->frame.subtype, 8u);

    EXPECT_TRUE(packet->frame.from_ds);
    EXPECT_FALSE(packet->frame.to_ds);
    EXPECT_TRUE(packet->frame.protected_frame);
    EXPECT_EQ(packet->frame.duration, 44u);
    EXPECT_EQ(packet->frame.sequence_number, 18u);
    EXPECT_EQ(packet->frame.qos_tid, 0u);
    EXPECT_EQ(packet->frame.header_size, 26u);
    EXPECT_EQ(packet->frame.body.size(), 108u);

    const Addresses roles = addresses(packet->frame);

    EXPECT_EQ(roles.da, (MacAddress {0x68, 0xa3, 0xc4, 0x03, 0x46, 0xda}));
    EXPECT_EQ(roles.bssid, (MacAddress {0x20, 0x7c, 0x8f, 0x50, 0x3f, 0x3a}));
    EXPECT_EQ(roles.sa, (MacAddress {0x20, 0x7c, 0x8f, 0x50, 0x3f, 0x3a}));
    EXPECT_EQ(failure_of(parse_llc_snap(packet->frame.body)), ErrorCode::not_supported);

    // the 8 bytes in front of the llc header are the ccmp header; this capture holds the payload already decrypted
    const Result<Llc> llc = parse_llc_snap(ArrUByteView(packet->frame.body.data() + 8, packet->frame.body.size() - 8));

    ASSERT_TRUE(llc.has_value());
    EXPECT_EQ(llc->ethertype, 0x0800u);
    EXPECT_EQ(llc->payload[0], 0x45);
}

TEST_F(TestDissect, test_dissect_real_null_data_frames_sent_with_mcs)
{
    const std::vector<uint8_t> mcs_2 = from_hex(real_null_data_mcs_2);
    const std::vector<uint8_t> mcs_11 = from_hex(real_null_data_mcs_11);
    const Result<Packet> slow = dissect_radiotap(ArrUByteView(mcs_2));
    const Result<Packet> fast = dissect_radiotap(ArrUByteView(mcs_11));

    ASSERT_TRUE(slow.has_value());
    ASSERT_TRUE(fast.has_value());
    ASSERT_TRUE(slow->radiotap->mcs.has_value());
    ASSERT_TRUE(fast->radiotap->mcs.has_value());
    EXPECT_EQ(slow->radiotap->mcs->index, 2u);
    EXPECT_EQ(slow->radiotap->mcs->bandwidth_mhz, 20u);
    EXPECT_FALSE(slow->radiotap->mcs->short_gi);
    EXPECT_EQ(slow->radiotap->mcs->rate_kbps, 19500u);
    EXPECT_EQ(fast->radiotap->mcs->index, 11u);
    EXPECT_EQ(fast->radiotap->mcs->rate_kbps, 52000u);
    EXPECT_EQ(slow->frame.subtype, 4u);
    EXPECT_TRUE(slow->frame.to_ds);
}

TEST_F(TestDissect, test_dissect_every_truncation_of_real_frames_is_handled)
{
    const std::vector<std::vector<uint8_t>> captures = {from_hex(real_probe_request),
                                                        from_hex(real_ack),
                                                        from_hex(real_probe_response),
                                                        from_hex(real_qos_data)};

    for (const std::vector<uint8_t> & capture : captures)
    {
        for (size_t length = 0; length <= capture.size(); ++length)
        {
            const Result<Packet> packet = dissect_radiotap(ArrUByteView(capture.data(), length));

            ASSERT_TRUE(graceful(packet));
            if (packet.has_value() == false)
                continue;
            EXPECT_TRUE(graceful(ies::elements(packet->frame.body)));
            EXPECT_TRUE(graceful(parse_beacon(packet->frame.body)));
            EXPECT_TRUE(graceful(parse_llc_snap(packet->frame.body)));
            ASSERT_TRUE(packet->radiotap.has_value());
            EXPECT_TRUE(graceful(dissect_80211(packet->radiotap->payload)));
        }
    }
}

} // namespace test
