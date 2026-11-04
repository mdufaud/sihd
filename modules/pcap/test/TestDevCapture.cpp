#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/core/Core.hpp>
#include <sihd/pcap/CapturedPacket.hpp>
#include <sihd/pcap/DevCapture.hpp>
#include <sihd/pcap/PcapWriter.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>

namespace test
{
using namespace sihd::pcap;
using namespace sihd::util;
using namespace sihd::core;
using namespace sihd::sys;

namespace
{

constexpr int datalink_ethernet = 1;

struct Recorded
{
        Timestamp capture_time;
        uint32_t original_length;
        int datalink;
        std::vector<uint8_t> data;
        Timestamp write_time;
};

} // namespace

class TestDevCapture: public ::testing::Test
{
    protected:
        TestDevCapture() { LoggerManager::stream(); }

        virtual ~TestDevCapture() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            _path = fs::combine(_tmp_dir.path(), "capture.pcap");
            _capture = _core.add_child<DevCapture>("capture");
        }

        bool write_file(const std::vector<std::pair<Timestamp, std::string>> & packets)
        {
            PcapWriter writer("writer");
            if (!writer.open(_path, datalink_ethernet))
                return false;
            for (const auto & [timestamp, bytes] : packets)
            {
                if (writer.write(bytes, timestamp) < 0)
                    return false;
            }
            return writer.close();
        }

        // records each packet and signals `reached` once the capture writes `status`
        void observe(Synchronizer & reached, DevCapture::Status status)
        {
            _packet_observer.set_method([this](Channel *channel) {
                const CapturedPacket
                    packet = CapturedPacket::decode(ArrUByteView(channel->data(), channel->byte_size())).value();
                _recorded.push_back({packet.timestamp(),
                                     packet.original_length(),
                                     packet.datalink(),
                                     std::vector<uint8_t>(packet.data().begin(), packet.data().end()),
                                     channel->timestamp()});
            });
            _status_observer.set_method([&reached, status](Channel *channel) {
                if (channel->read<uint8_t>(0) == static_cast<uint8_t>(status))
                    reached.sync();
            });
            _capture->get_channel("packet").value()->add_observer(&_packet_observer);
            _capture->get_channel("status").value()->add_observer(&_status_observer);
        }

        void activate() { ASSERT_TRUE(_capture->get_channel("activate").value()->write<bool>(0, true)); }

        TmpDir _tmp_dir;
        std::string _path;
        Core _core;
        DevCapture *_capture;
        Handler<Channel *> _packet_observer;
        Handler<Channel *> _status_observer;
        std::vector<Recorded> _recorded;
};

TEST_F(TestDevCapture, test_devcapture_replays_a_file_once_activated)
{
    ASSERT_TRUE(this->write_file({{Timestamp(time::sec(1)), "first"}, {Timestamp(time::sec(2)), "second"}}));
    ASSERT_TRUE(_capture->set_file(_path));
    ASSERT_TRUE(_core.init());
    Synchronizer paused(2);
    Synchronizer finished(2);
    Handler<Channel *> pause_observer([&paused](Channel *channel) {
        if (channel->read<uint8_t>(0) == static_cast<uint8_t>(DevCapture::Status::paused))
            paused.sync();
    });
    _capture->get_channel("status").value()->add_observer(&pause_observer);
    this->observe(finished, DevCapture::Status::end_of_file);

    ASSERT_TRUE(_core.start());
    ASSERT_TRUE(paused.sync(Duration(time::sec(2))));
    EXPECT_TRUE(_recorded.empty());
    this->activate();
    ASSERT_TRUE(finished.sync(Duration(time::sec(2))));

    ASSERT_EQ(_recorded.size(), 2u);
    EXPECT_EQ(_recorded[1].capture_time, Timestamp(time::sec(2)));
    EXPECT_EQ(_recorded[1].original_length, 6u);
    EXPECT_EQ(_recorded[1].datalink, datalink_ethernet);
    EXPECT_EQ(std::string(_recorded[1].data.begin(), _recorded[1].data.end()), "second");
    EXPECT_EQ(_capture->get_channel("datalink").value()->read<int32_t>(0), datalink_ethernet);
    EXPECT_TRUE(_core.stop());
}

TEST_F(TestDevCapture, test_devcapture_realtime_keeps_the_capture_pace)
{
    ASSERT_TRUE(
        this->write_file({{Timestamp(time::sec(1)), "first"}, {Timestamp(time::sec(1) + time::milli(200)), "second"}}));
    ASSERT_TRUE(_capture->set_file(_path));
    ASSERT_TRUE(_capture->set_realtime(true));
    ASSERT_TRUE(_core.init());
    Synchronizer finished(2);
    this->observe(finished, DevCapture::Status::end_of_file);

    ASSERT_TRUE(_core.start());
    this->activate();
    ASSERT_TRUE(finished.sync(Duration(time::sec(2))));

    ASSERT_EQ(_recorded.size(), 2u);
    EXPECT_GE(_recorded[1].write_time - _recorded[0].write_time, time::milli(150));
    EXPECT_TRUE(_core.stop());
}

TEST_F(TestDevCapture, test_devcapture_stop_interrupts_a_realtime_wait)
{
    ASSERT_TRUE(this->write_file({{Timestamp(time::sec(1)), "first"}, {Timestamp(time::sec(1000)), "second"}}));
    ASSERT_TRUE(_capture->set_file(_path));
    ASSERT_TRUE(_capture->set_realtime(true));
    ASSERT_TRUE(_core.init());
    Synchronizer running(2);
    this->observe(running, DevCapture::Status::running);

    ASSERT_TRUE(_core.start());
    this->activate();
    ASSERT_TRUE(running.sync(Duration(time::sec(2))));
    const Timestamp stop_begin = Timestamp::now();

    EXPECT_TRUE(_core.stop());
    EXPECT_LT(Timestamp::now() - stop_begin, time::sec(2));
    EXPECT_LE(_recorded.size(), 1u);
}

TEST_F(TestDevCapture, test_devcapture_fails_on_a_truncated_file)
{
    ASSERT_TRUE(this->write_file({{Timestamp(time::sec(1)), "first"}, {Timestamp(time::sec(2)), "second"}}));
    const uint64_t size = fs::file_size(_path).value();
    ASSERT_TRUE(fs::truncate(_path, static_cast<int64_t>(size) - 3));
    ASSERT_TRUE(_capture->set_file(_path));
    ASSERT_TRUE(_core.init());
    Synchronizer failed(2);
    this->observe(failed, DevCapture::Status::failed);

    ASSERT_TRUE(_core.start());
    this->activate();
    ASSERT_TRUE(failed.sync(Duration(time::sec(2))));

    ASSERT_EQ(_recorded.size(), 1u);
    EXPECT_EQ(std::string(_recorded[0].data.begin(), _recorded[0].data.end()), "first");
    EXPECT_TRUE(_core.stop());
}

TEST_F(TestDevCapture, test_devcapture_live_settings_refuse_negative_periods)
{
    EXPECT_FALSE(_capture->set_timeout(-1));
    EXPECT_FALSE(_capture->set_stats_period(0));
    EXPECT_TRUE(_capture->set_timeout(0));
    EXPECT_TRUE(_capture->set_stats_period(10));
}

TEST_F(TestDevCapture, test_devcapture_needs_one_source)
{
    ASSERT_TRUE(_core.init());

    EXPECT_FALSE(_core.start());
    EXPECT_TRUE(_capture->set_file(_path));
    EXPECT_FALSE(_capture->set_interface("eth0"));
}

} // namespace test
