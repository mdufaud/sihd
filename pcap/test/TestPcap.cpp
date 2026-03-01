#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>

#include <sihd/pcap/PcapReader.hpp>
#include <sihd/pcap/PcapWriter.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::pcap;
using namespace sihd::util;
using namespace sihd::sys;
class TestPcap: public ::testing::Test
{
    protected:
        TestPcap() { sihd::util::LoggerManager::stream(); }

        virtual ~TestPcap() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPcap, test_pcap_reader)
{
    PcapReader reader("pcap-reader");

    std::string path = "test/resources/tcp/anon.pcappng";
    SIHD_LOG(info, "Reading pcap: {}", path);

    EXPECT_TRUE(reader.open(path));
    EXPECT_EQ(reader.snaplen(), 65535);
    EXPECT_EQ(reader.datalink(), DLT_EN10MB);
    EXPECT_EQ(reader.major_version(), 1);
    EXPECT_EQ(reader.minor_version(), 0);
    EXPECT_EQ(reader.is_swapped(), false);
    EXPECT_NE(reader.file(), nullptr);
    sihd::util::ArrCharView view;
    size_t n = 0;
    while (reader.read_next())
    {
        const struct pcap_pkthdr *hdr = reader.packet_header();
        EXPECT_TRUE(reader.get_read_data(view));
        EXPECT_EQ(hdr->len, view.size());
        ++n;
    }
    SIHD_TRACE("{} packets read", n);
    EXPECT_EQ(n, 35u);
    EXPECT_TRUE(reader.close());
}

TEST_F(TestPcap, test_pcap_writer)
{
    TmpDir tmp_dir;

    char hw[] = "hello world";
    PcapWriter writer("pcap-writer");

    std::string path = fs::combine(tmp_dir.path(), "test.pcap");
    SIHD_LOG(info, "Writing pcap: {}", path);

    EXPECT_TRUE(writer.open(path, DLT_EN10MB));
    EXPECT_EQ(writer.snaplen(), 65535);
    EXPECT_EQ(writer.datalink(), DLT_EN10MB);
    EXPECT_TRUE(writer.write(hw, sizeof(hw)));
    EXPECT_TRUE(writer.close());

    std::string content = fs::read_all(path).value();
    EXPECT_EQ(memcmp(content.c_str() + content.size() - sizeof(hw), hw, sizeof(hw)), 0);
}
} // namespace test
