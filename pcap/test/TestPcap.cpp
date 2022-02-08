#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/pcap/PcapReader.hpp>
#include <sihd/pcap/PcapWriter.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::pcap;
    using namespace sihd::util;
    class TestPcap:   public ::testing::Test
    {
        protected:
            TestPcap()
            {
                sihd::util::LoggerManager::basic();
                sihd::util::Files::make_directories(_base_test_dir);
            }

            virtual ~TestPcap()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _base_test_dir = sihd::util::Files::combine({getenv("TEST_PATH"), "pcap"});
    };

    TEST_F(TestPcap, test_pcap_reader)
    {
        PcapReader reader("pcap-reader");

        std::string path = "test/resources/tcp/anon.pcappng";
        SIHD_LOG(info, "Reading pcap: " << path);

        EXPECT_TRUE(reader.open(path));
        EXPECT_EQ(reader.snaplen(), 65535);
        EXPECT_EQ(reader.datalink(), DLT_EN10MB);
        EXPECT_EQ(reader.major_version(), 1);
        EXPECT_EQ(reader.minor_version(), 0);
        EXPECT_EQ(reader.is_swapped(), false);
        EXPECT_NE(reader.file(), nullptr);
        char *data;
        size_t size;
        size_t n = 0;
        while (reader.read_next())
        {
            const struct pcap_pkthdr *hdr = reader.packet_header();
            EXPECT_TRUE(reader.get_read_data(&data, &size));
            EXPECT_EQ(hdr->len, size);
            ++n;
        }
        SIHD_TRACE(n << " packets read");
        EXPECT_EQ(n, 35u);
        EXPECT_TRUE(reader.close());
    }

    TEST_F(TestPcap, test_pcap_writer)
    {
        char hw[] = "hello world";
        PcapWriter writer("pcap-writer");

        std::string path = Files::combine(_base_test_dir, "test.pcap");
        SIHD_LOG(info, "Writing pcap: " << path);

        EXPECT_TRUE(writer.open(path, DLT_EN10MB));
        EXPECT_EQ(writer.snaplen(), 65535);
        EXPECT_EQ(writer.datalink(), DLT_EN10MB);
        EXPECT_TRUE(writer.write(hw, sizeof(hw)));
        EXPECT_TRUE(writer.close());

        std::string content = Files::read_all(path).value();
        EXPECT_EQ(memcmp(content.c_str() + content.size() - sizeof(hw), hw, sizeof(hw)), 0);
    }
}