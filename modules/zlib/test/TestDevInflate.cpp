#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <sihd/core/Core.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/zlib/DevInflate.hpp>

#include "test_helper.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
using namespace sihd::util;
using namespace sihd::zlib;

class TestDevInflate: public ::testing::Test
{
    protected:
        TestDevInflate(): _restored_observer([this](Channel *) { ++_restored_writes; }) { LoggerManager::stream(); }

        virtual ~TestDevInflate() { LoggerManager::clear_loggers(); }

        void start(Core & core, size_t output_capacity)
        {
            _packets = core.add_channel_resizable("packets", TYPE_BYTE, 0, 4096);
            DevInflate *inflate = core.add_child<DevInflate>("inflate");
            ASSERT_TRUE(inflate->set_conf_str("mode", "message"));
            ASSERT_TRUE(inflate->set_conf_int("output_capacity", static_cast<int64_t>(output_capacity)));
            ASSERT_TRUE(inflate->add_link("packets", "..packets"));
            ASSERT_TRUE(core.init());
            ASSERT_TRUE(core.start());
            auto restored = core.find_channel("inflate.packets_out");
            ASSERT_TRUE(restored.has_value()) << restored.error().message;
            _restored = *restored;
            _restored->add_observer(&_restored_observer);
        }

        Channel *_packets = nullptr;
        Channel *_restored = nullptr;
        int _restored_writes = 0;
        Handler<Channel *> _restored_observer;
};

TEST_F(TestDevInflate, test_dev_inflate_drops_a_corrupted_message)
{
    Core core;
    this->start(core, 4096);
    std::string corrupted = compress_message("a damaged message");
    corrupted[corrupted.size() / 2] ^= 0x55;

    ASSERT_TRUE(_packets->write(ArrByteView(corrupted)));
    ASSERT_TRUE(_packets->write(ArrByteView(compress_message("an intact message"))));

    EXPECT_EQ(_restored_writes, 1);
    EXPECT_EQ(view_of(_restored), "an intact message");
    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());
}

TEST_F(TestDevInflate, test_dev_inflate_drops_a_message_larger_than_its_output)
{
    Core core;
    this->start(core, 100);

    ASSERT_TRUE(_packets->write(ArrByteView(compress_message(std::string(1000, 'x')))));
    ASSERT_TRUE(_packets->write(ArrByteView(compress_message("fits"))));

    EXPECT_EQ(_restored_writes, 1);
    EXPECT_EQ(view_of(_restored), "fits");
    EXPECT_EQ(_restored->capacity(), 100u);
    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());
}

TEST_F(TestDevInflate, test_dev_inflate_rejects_unknown_confs)
{
    DevInflate inflate("inflate");

    EXPECT_FALSE(inflate.set_conf_str("mode", "finish").has_value());
    EXPECT_FALSE(inflate.set_conf_int("output_capacity", 0).has_value());
}

} // namespace test
