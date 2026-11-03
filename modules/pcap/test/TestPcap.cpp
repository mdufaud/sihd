#include <pcap.h>

#include <array>

#include <gtest/gtest.h>

#include <sihd/pcap/PcapReader.hpp>
#include <sihd/pcap/PcapWriter.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::pcap;
using namespace sihd::util;
using namespace sihd::sys;
class TestPcap: public ::testing::Test
{
    protected:
        TestPcap() { LoggerManager::stream(); }

        virtual ~TestPcap() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPcap, test_pcap_reader)
{
    PcapReader reader("pcap-reader");

    std::string path = "test/resources/tcp/anon.pcappng";
    SIHD_LOG(info, "Reading pcap: {}", path);

    EXPECT_TRUE(reader.open(path).has_value());

    EXPECT_EQ(reader.snaplen(), 65535);
    EXPECT_EQ(reader.datalink(), DLT_EN10MB);
    EXPECT_EQ(reader.major_version(), 1);
    EXPECT_EQ(reader.minor_version(), 0);
    EXPECT_EQ(reader.is_swapped(), false);
    EXPECT_NE(reader.file(), nullptr);

    ArrCharView view;
    size_t packet_count = 0;
    while (true)
    {
        auto next = reader.read_next();
        ASSERT_TRUE(next.has_value());
        if (next.value() == false)
            break;
        EXPECT_TRUE(reader.get_read_data(view));
        EXPECT_EQ(reader.packet_caplen(), view.size());
        ++packet_count;
    }

    SIHD_TRACE("{} packets read", packet_count);
    EXPECT_EQ(packet_count, 35u);
    EXPECT_TRUE(reader.close());
}

TEST_F(TestPcap, test_pcap_reader_truncated_capture)
{
    TmpDir tmp_dir;
    const std::string path = fs::combine(tmp_dir.path(), "truncated.pcap");
    // global header (little endian, v2.4, snaplen 4, ethernet), then one packet of caplen 4 and len 8
    const std::array<uint8_t, 44> capture {0xd4, 0xc3, 0xb2, 0xa1, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
                                           0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 'a',  'b',  'c',  'd'};
    ASSERT_TRUE(fs::write_binary(path, {reinterpret_cast<const char *>(capture.data()), capture.size()}).has_value());

    PcapReader reader("pcap-reader");
    ASSERT_TRUE(reader.open(path).has_value());
    auto next = reader.read_next();
    ASSERT_TRUE(next.has_value());
    ASSERT_TRUE(next.value());

    ArrCharView view;
    ASSERT_TRUE(reader.get_read_data(view));
    EXPECT_EQ(reader.packet_len(), 8u);
    EXPECT_EQ(reader.packet_caplen(), 4u);
    EXPECT_EQ(view.size(), 4u);
    EXPECT_EQ(std::string(view.data(), view.size()), "abcd");
}

TEST_F(TestPcap, test_pcap_writer)
{
    TmpDir tmp_dir;

    std::array<char, 12> hw {"hello world"};
    PcapWriter writer("pcap-writer");

    std::string path = fs::combine(tmp_dir.path(), "test.pcap");
    SIHD_LOG(info, "Writing pcap: {}", path);

    EXPECT_TRUE(writer.open(path, DLT_EN10MB).has_value());
    EXPECT_EQ(writer.snaplen(), 65535);
    EXPECT_EQ(writer.datalink(), DLT_EN10MB);
    EXPECT_TRUE(writer.write({hw.data(), hw.size()}));
    EXPECT_TRUE(writer.close());

    std::string content = fs::read_all(path, true).value();
    EXPECT_EQ(memcmp(content.c_str() + content.size() - hw.size(), hw.data(), hw.size()), 0);
}

TEST_F(TestPcap, test_pcap_writer_timestamp_round_trip)
{
    TmpDir tmp_dir;
    const std::string path = fs::combine(tmp_dir.path(), "timestamp.pcap");
    const std::array<char, 4> data {'a', 'b', 'c', 'd'};

    PcapWriter writer("pcap-writer");
    ASSERT_TRUE(writer.open(path, DLT_RAW).has_value());
    EXPECT_TRUE(writer.write({data.data(), data.size()}, Timestamp(1700000000123456789)));
    EXPECT_TRUE(writer.close());

    PcapReader reader("pcap-reader");
    ASSERT_TRUE(reader.open(path).has_value());
    auto next = reader.read_next();
    ASSERT_TRUE(next.has_value());
    ASSERT_TRUE(next.value());
    auto timestamp = reader.get_read_timestamp();
    ASSERT_TRUE(timestamp.has_value());
    EXPECT_EQ(timestamp.value().get(), 1700000000123456000);
}
} // namespace test
