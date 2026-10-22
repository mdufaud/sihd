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
        EXPECT_EQ(reader.packet_len(), view.size());
        ++packet_count;
    }

    SIHD_TRACE("{} packets read", packet_count);
    EXPECT_EQ(packet_count, 35u);
    EXPECT_TRUE(reader.close());
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
} // namespace test
