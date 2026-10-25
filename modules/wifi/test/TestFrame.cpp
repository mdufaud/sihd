#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/wifi/frame.hpp>

#include "wifi_test_helpers.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::wifi;

class TestFrame: public ::testing::Test
{
    protected:
        TestFrame() { LoggerManager::stream(); }

        virtual ~TestFrame() { LoggerManager::clear_loggers(); }

        static constexpr MacAddress mac_a = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
        static constexpr MacAddress mac_b = {0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb};
        static constexpr MacAddress mac_c = {0xcc, 0xdd, 0xee, 0xff, 0x01, 0x02};
        static constexpr MacAddress mac_d = {0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
        static constexpr MacAddress broadcast = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

        static std::vector<uint8_t> header_of(uint8_t byte0, uint8_t byte1)
        {
            std::vector<uint8_t> bytes = {byte0, byte1, 0x00, 0x00};
            bytes.insert(bytes.end(), mac_a.begin(), mac_a.end());
            bytes.insert(bytes.end(), mac_b.begin(), mac_b.end());
            bytes.insert(bytes.end(), mac_c.begin(), mac_c.end());
            bytes.insert(bytes.end(), {0x00, 0x00});
            return bytes;
        }

        static Result<Frame> parse(const std::vector<uint8_t> & bytes) { return parse_frame(ArrUByteView(bytes)); }

        static ErrorCode failure(uint8_t byte0, uint8_t byte1, size_t size)
        {
            std::vector<uint8_t> bytes(size, 0x00);
            bytes[0] = byte0;
            bytes[1] = byte1;
            const Result<Frame> frame = parse(bytes);
            return frame.has_value() ? ErrorCode::none : frame.error().code;
        }
};

TEST_F(TestFrame, test_frame_beacon)
{
    std::vector<uint8_t> bytes = {0x80, 0x00, 0x00, 0x00};
    bytes.insert(bytes.end(), broadcast.begin(), broadcast.end());
    bytes.insert(bytes.end(), mac_b.begin(), mac_b.end());
    bytes.insert(bytes.end(), mac_b.begin(), mac_b.end());
    bytes.insert(bytes.end(), {0x30, 0x12, 0x01, 0x02, 0x03, 0x04});

    const Result<Frame> frame = parse(bytes);

    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->type, Frame::Type::management);
    EXPECT_EQ(frame->subtype, 8u);
    EXPECT_EQ(frame->addr1, broadcast);
    EXPECT_EQ(frame->addr2, mac_b);
    EXPECT_EQ(frame->addr3, mac_b);
    EXPECT_FALSE(frame->addr4.has_value());
    EXPECT_EQ(frame->sequence_number, 0x123u);
    EXPECT_EQ(frame->fragment_number, 0u);
    EXPECT_EQ(frame->header_size, 24u);
    ASSERT_EQ(frame->body.size(), 4u);
    EXPECT_EQ(frame->body[0], 0x01);
    EXPECT_FALSE(frame->ht_control);
}

TEST_F(TestFrame, test_frame_addresses_by_distribution_system)
{
    const std::vector<uint8_t> ibss = header_of(0x08, 0x00);
    const std::vector<uint8_t> to_ap = header_of(0x08, 0x01);
    const std::vector<uint8_t> from_ap = header_of(0x08, 0x02);

    const Result<Frame> ibss_frame = parse(ibss);
    const Result<Frame> to_ap_frame = parse(to_ap);
    const Result<Frame> from_ap_frame = parse(from_ap);

    ASSERT_TRUE(ibss_frame.has_value());
    ASSERT_TRUE(to_ap_frame.has_value());
    ASSERT_TRUE(from_ap_frame.has_value());
    EXPECT_EQ(addresses(*ibss_frame).da, mac_a);
    EXPECT_EQ(addresses(*ibss_frame).sa, mac_b);
    EXPECT_EQ(addresses(*ibss_frame).bssid, mac_c);
    EXPECT_EQ(addresses(*to_ap_frame).bssid, mac_a);
    EXPECT_EQ(addresses(*to_ap_frame).sa, mac_b);
    EXPECT_EQ(addresses(*to_ap_frame).da, mac_c);
    EXPECT_EQ(addresses(*from_ap_frame).da, mac_a);
    EXPECT_EQ(addresses(*from_ap_frame).bssid, mac_b);
    EXPECT_EQ(addresses(*from_ap_frame).sa, mac_c);
    EXPECT_EQ(addresses(*from_ap_frame).ra, mac_a);
    EXPECT_EQ(addresses(*from_ap_frame).ta, mac_b);
}

TEST_F(TestFrame, test_frame_wds_has_four_addresses)
{
    std::vector<uint8_t> wds = header_of(0x08, 0x03);
    wds.insert(wds.end(), mac_d.begin(), mac_d.end());
    std::vector<uint8_t> qos_wds = header_of(0x88, 0x03);
    qos_wds.insert(qos_wds.end(), mac_d.begin(), mac_d.end());
    qos_wds.insert(qos_wds.end(), {0x07, 0x00, 0xaa});

    const Result<Frame> frame = parse(wds);
    const Result<Frame> qos_frame = parse(qos_wds);

    ASSERT_TRUE(frame.has_value());
    ASSERT_TRUE(qos_frame.has_value());
    EXPECT_EQ(frame->header_size, 30u);
    EXPECT_EQ(frame->addr4, mac_d);
    EXPECT_EQ(addresses(*frame).da, mac_c);
    EXPECT_EQ(addresses(*frame).sa, mac_d);
    EXPECT_FALSE(addresses(*frame).bssid.has_value());
    EXPECT_EQ(qos_frame->header_size, 32u);
    EXPECT_EQ(qos_frame->qos_tid, 7u);
    ASSERT_EQ(qos_frame->body.size(), 1u);
    EXPECT_EQ(qos_frame->body[0], 0xaa);
}

TEST_F(TestFrame, test_frame_qos_data)
{
    std::vector<uint8_t> bytes = header_of(0x88, 0x01);
    bytes.insert(bytes.end(), {0x05, 0x00, 0xde, 0xad});
    std::vector<uint8_t> aggregated = header_of(0x88, 0x01);
    aggregated.insert(aggregated.end(), {0x85, 0x00, 0xde, 0xad});

    const Result<Frame> frame = parse(bytes);
    const Result<Frame> aggregate = parse(aggregated);

    ASSERT_TRUE(frame.has_value());
    ASSERT_TRUE(aggregate.has_value());
    EXPECT_FALSE(frame->amsdu);
    EXPECT_TRUE(aggregate->amsdu);

    EXPECT_EQ(addresses(*frame).da, mac_c);
    EXPECT_FALSE(addresses(*aggregate).da.has_value());
    EXPECT_FALSE(addresses(*aggregate).sa.has_value());
    EXPECT_EQ(addresses(*aggregate).bssid, mac_a);
    EXPECT_EQ(aggregate->qos_tid, 5u);

    EXPECT_EQ(frame->type, Frame::Type::data);
    EXPECT_EQ(frame->subtype, 8u);
    EXPECT_EQ(frame->header_size, 26u);
    EXPECT_EQ(frame->qos_tid, 5u);
    EXPECT_EQ(frame->body.size(), 2u);
    EXPECT_FALSE(frame->ht_control);
}

TEST_F(TestFrame, test_frame_ht_control_follows_the_order_bit)
{
    std::vector<uint8_t> qos = header_of(0x88, 0x81);
    qos.insert(qos.end(), {0x03, 0x00, 0x01, 0x02, 0x03, 0x04, 0xbb});
    std::vector<uint8_t> beacon = header_of(0x80, 0x80);
    beacon.insert(beacon.end(), {0x01, 0x02, 0x03, 0x04, 0xcc});
    const std::vector<uint8_t> plain_ordered = header_of(0x08, 0x80);

    const Result<Frame> qos_frame = parse(qos);
    const Result<Frame> beacon_frame = parse(beacon);
    const Result<Frame> plain_frame = parse(plain_ordered);

    ASSERT_TRUE(qos_frame.has_value());
    ASSERT_TRUE(beacon_frame.has_value());
    ASSERT_TRUE(plain_frame.has_value());
    EXPECT_TRUE(qos_frame->ht_control);
    EXPECT_EQ(qos_frame->header_size, 30u);
    EXPECT_EQ(qos_frame->qos_tid, 3u);
    EXPECT_EQ(qos_frame->body[0], 0xbb);
    EXPECT_TRUE(beacon_frame->ht_control);
    EXPECT_EQ(beacon_frame->header_size, 28u);
    EXPECT_EQ(beacon_frame->body[0], 0xcc);
    EXPECT_TRUE(plain_frame->order);
    EXPECT_FALSE(plain_frame->ht_control);
    EXPECT_EQ(plain_frame->header_size, 24u);
}

TEST_F(TestFrame, test_frame_control_frames)
{
    std::vector<uint8_t> ack = {0xd4, 0x00, 0x3c, 0x00};
    ack.insert(ack.end(), mac_a.begin(), mac_a.end());
    std::vector<uint8_t> rts = {0xb4, 0x00, 0x00, 0x00};
    rts.insert(rts.end(), mac_a.begin(), mac_a.end());
    rts.insert(rts.end(), mac_b.begin(), mac_b.end());
    std::vector<uint8_t> ps_poll = {0xa4, 0x00, 0x01, 0xc0};
    ps_poll.insert(ps_poll.end(), mac_a.begin(), mac_a.end());
    ps_poll.insert(ps_poll.end(), mac_b.begin(), mac_b.end());
    std::vector<uint8_t> cf_end = {0xe4, 0x00, 0x00, 0x00};
    cf_end.insert(cf_end.end(), broadcast.begin(), broadcast.end());
    cf_end.insert(cf_end.end(), mac_b.begin(), mac_b.end());

    const Result<Frame> ack_frame = parse(ack);
    const Result<Frame> rts_frame = parse(rts);
    const Result<Frame> ps_poll_frame = parse(ps_poll);
    const Result<Frame> cf_end_frame = parse(cf_end);

    ASSERT_TRUE(ack_frame.has_value());
    ASSERT_TRUE(rts_frame.has_value());
    ASSERT_TRUE(ps_poll_frame.has_value());
    ASSERT_TRUE(cf_end_frame.has_value());
    EXPECT_EQ(ack_frame->type, Frame::Type::control);
    EXPECT_EQ(ack_frame->subtype, 13u);
    EXPECT_EQ(ack_frame->duration, 60u);
    EXPECT_EQ(ack_frame->header_size, 10u);
    EXPECT_FALSE(ack_frame->addr2.has_value());
    EXPECT_FALSE(ack_frame->sequence_number.has_value());
    EXPECT_EQ(addresses(*ack_frame).ra, mac_a);

    EXPECT_EQ(rts_frame->header_size, 16u);
    EXPECT_EQ(addresses(*rts_frame).ra, mac_a);
    EXPECT_EQ(addresses(*rts_frame).ta, mac_b);
    EXPECT_FALSE(addresses(*rts_frame).bssid.has_value());

    EXPECT_EQ(ps_poll_frame->duration, 0xc001u);
    EXPECT_EQ(addresses(*ps_poll_frame).bssid, mac_a);
    EXPECT_EQ(addresses(*cf_end_frame).ra, broadcast);
    EXPECT_EQ(addresses(*cf_end_frame).bssid, mac_b);
}

TEST_F(TestFrame, test_frame_control_wrapper_and_subtype_enums)
{
    std::vector<uint8_t> wrapper = {0x74, 0x00, 0x00, 0x00};
    wrapper.insert(wrapper.end(), mac_a.begin(), mac_a.end());
    wrapper.insert(wrapper.end(), {0xb4, 0x00, 0x01, 0x02, 0x03, 0x04});
    const std::vector<uint8_t> beacon = header_of(0x80, 0x00);
    const std::vector<uint8_t> reserved = header_of(0x70, 0x00);
    const std::vector<uint8_t> tack = header_of(0x34, 0x00);
    const std::vector<uint8_t> extension = header_of(0x64, 0x00);
    const std::vector<uint8_t> reserved_control = header_of(0x14, 0x00);
    std::vector<uint8_t> qos_data = header_of(0x88, 0x00);
    qos_data.insert(qos_data.end(), {0x00, 0x00});

    const Result<Frame> wrapped = parse(wrapper);
    const Result<Frame> announcing = parse(beacon);
    const Result<Frame> unknown = parse(reserved);
    const Result<Frame> tack_frame = parse(tack);
    const Result<Frame> extension_frame = parse(extension);
    const Result<Frame> reserved_control_frame = parse(reserved_control);
    const Result<Frame> data = parse(qos_data);

    ASSERT_TRUE(wrapped.has_value());
    ASSERT_TRUE(announcing.has_value());
    ASSERT_TRUE(unknown.has_value());
    ASSERT_TRUE(tack_frame.has_value());
    ASSERT_TRUE(extension_frame.has_value());
    ASSERT_TRUE(reserved_control_frame.has_value());
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(wrapped->header_size, 16u);
    EXPECT_EQ(wrapped->addr1, mac_a);
    EXPECT_FALSE(wrapped->addr2.has_value());
    EXPECT_FALSE(addresses(*wrapped).ta.has_value());
    EXPECT_EQ(control_subtype(*wrapped), Frame::ControlSubtype::control_wrapper);
    EXPECT_FALSE(management_subtype(*wrapped).has_value());
    EXPECT_EQ(management_subtype(*announcing), Frame::ManagementSubtype::beacon);

    EXPECT_FALSE(control_subtype(*announcing).has_value());
    EXPECT_FALSE(management_subtype(*unknown).has_value());
    EXPECT_FALSE(management_subtype(*data).has_value());
    EXPECT_EQ(control_subtype(*tack_frame), Frame::ControlSubtype::tack);
    EXPECT_EQ(control_subtype(*extension_frame), Frame::ControlSubtype::control_frame_extension);
    EXPECT_FALSE(control_subtype(*reserved_control_frame).has_value());
}

TEST_F(TestFrame, test_frame_control_flags)
{
    const std::vector<uint8_t> most_flags_set = header_of(0x08, 0x7c);
    const std::vector<uint8_t> no_flags = header_of(0x08, 0x00);

    const Result<Frame> set = parse(most_flags_set);
    const Result<Frame> unset = parse(no_flags);

    ASSERT_TRUE(set.has_value());
    ASSERT_TRUE(unset.has_value());
    EXPECT_TRUE(set->more_fragments);
    EXPECT_TRUE(set->retry);
    EXPECT_TRUE(set->power_management);
    EXPECT_TRUE(set->more_data);
    EXPECT_TRUE(set->protected_frame);
    EXPECT_FALSE(set->order);
    EXPECT_FALSE(set->to_ds);
    EXPECT_FALSE(unset->more_fragments);
    EXPECT_FALSE(unset->retry);
    EXPECT_FALSE(unset->power_management);
    EXPECT_FALSE(unset->more_data);
    EXPECT_FALSE(unset->protected_frame);
}

TEST_F(TestFrame, test_frame_mac_helpers)
{
    EXPECT_TRUE(is_group(broadcast));
    EXPECT_TRUE(is_group(MacAddress {0x01, 0x00, 0x5e, 0x00, 0x00, 0x01}));
    EXPECT_TRUE(is_group(MacAddress {0x33, 0x33, 0x00, 0x00, 0x00, 0x01}));
    EXPECT_FALSE(is_group(mac_a));
    EXPECT_TRUE(is_locally_administered(MacAddress {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}));
    EXPECT_TRUE(is_locally_administered(MacAddress {0xda, 0xa1, 0x19, 0x00, 0x00, 0x01}));
    EXPECT_FALSE(is_locally_administered(mac_a));
    EXPECT_EQ(parse_mac("00:11:22:33:44:55"), mac_a);
    EXPECT_EQ(parse_mac("fF:ff:Ff:ff:FF:ff"), broadcast);
    EXPECT_EQ(failure_of(parse_mac("00:11:22:33:44")), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(parse_mac("00-11-22-33-44-55")), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(parse_mac("00:11:22:33:44:5g")), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(parse_mac("+0:11:22:33:44:55")), ErrorCode::invalid_argument);
}

TEST_F(TestFrame, test_frame_rejects_malformed_frames)
{
    EXPECT_EQ(failure(0x80, 0x00, 9), ErrorCode::invalid_argument);
    EXPECT_EQ(failure(0x81, 0x00, 24), ErrorCode::invalid_argument);
    EXPECT_EQ(failure(0x80, 0x00, 23), ErrorCode::invalid_argument);
    EXPECT_EQ(failure(0x88, 0x00, 25), ErrorCode::invalid_argument);
    EXPECT_EQ(failure(0x08, 0x03, 29), ErrorCode::invalid_argument);
    EXPECT_EQ(failure(0xb4, 0x00, 15), ErrorCode::invalid_argument);
    EXPECT_EQ(failure(0x0c, 0x00, 24), ErrorCode::not_supported);
}

} // namespace test
