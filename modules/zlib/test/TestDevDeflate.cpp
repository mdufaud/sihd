#include <array>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <sihd/core/Core.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/zlib/DevDeflate.hpp>
#include <sihd/zlib/DevInflate.hpp>
#include <sihd/zlib/Inflater.hpp>

#include "test_helper.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
using namespace sihd::util;
using namespace sihd::zlib;

class TestDevDeflate: public ::testing::Test
{
    protected:
        TestDevDeflate() { LoggerManager::stream(); }

        virtual ~TestDevDeflate() { LoggerManager::clear_loggers(); }

        // source -> deflate -> inflate, the consumer added before its producer
        DevDeflate *wire(Core & core, const std::string & mode)
        {
            _source = core.add_channel_resizable("source", TYPE_BYTE, 0, source_capacity);
            DevInflate *inflate = core.add_child<DevInflate>("inflate");
            DevDeflate *deflate = core.add_child<DevDeflate>("deflate");
            EXPECT_TRUE(deflate->set_conf_str("mode", mode));
            EXPECT_TRUE(inflate->set_conf_str("mode", mode));
            EXPECT_TRUE(deflate->add_link("source", "..source"));
            EXPECT_TRUE(inflate->add_link("compressed", "..deflate.source_out"));
            return deflate;
        }

        void start(Core & core)
        {
            ASSERT_TRUE(core.init());
            ASSERT_TRUE(core.start());
            auto compressed = core.find_channel("deflate.source_out");
            auto restored = core.find_channel("inflate.compressed_out");
            ASSERT_TRUE(compressed.has_value()) << compressed.error().message;
            ASSERT_TRUE(restored.has_value()) << restored.error().message;
            _compressed = *compressed;
            _restored = *restored;
        }

        static constexpr size_t source_capacity = 4096;

        Channel *_source = nullptr;
        Channel *_compressed = nullptr;
        Channel *_restored = nullptr;
};

TEST_F(TestDevDeflate, test_dev_deflate_stream_round_trip)
{
    Core core;
    this->wire(core, "stream");
    this->start(core);
    const std::array<std::string, 3> writes = {"first write", text_of(3000), "last write"};

    for (const std::string & payload : writes)
    {
        ASSERT_TRUE(_source->write(ArrByteView(payload)));

        EXPECT_EQ(view_of(_restored), payload);
    }
    EXPECT_LT(_compressed->byte_size(), 50u);
    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());
}

TEST_F(TestDevDeflate, test_dev_deflate_message_writes_are_complete_streams)
{
    Core core;
    this->wire(core, "message");
    this->start(core);
    const std::string payload = text_of(1000);

    ASSERT_TRUE(_source->write(ArrByteView(payload)));
    ASSERT_TRUE(_source->write(ArrByteView(payload)));

    Inflater fresh_inflater(source_capacity);
    Result<ArrByte> alone = fresh_inflater.inflate_message(ArrByteView(view_of(_compressed)));
    ASSERT_TRUE(alone.has_value()) << alone.error().message;
    EXPECT_EQ(alone->cpp_str_view(), payload);
    EXPECT_EQ(view_of(_restored), payload);
    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());
}

TEST_F(TestDevDeflate, test_dev_deflate_compresses_each_link_on_its_own_stream)
{
    Core core;
    Channel *first = core.add_channel_resizable("first", TYPE_BYTE, 0, source_capacity);
    Channel *second = core.add_channel_resizable("second", TYPE_BYTE, 0, source_capacity);
    DevDeflate *deflate = core.add_child<DevDeflate>("deflate");
    ASSERT_TRUE(deflate->add_link("first", "..first"));
    ASSERT_TRUE(deflate->add_link("second", "..second"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    auto first_out = core.find_channel("deflate.first_out");
    auto second_out = core.find_channel("deflate.second_out");
    ASSERT_TRUE(first_out.has_value()) << first_out.error().message;
    ASSERT_TRUE(second_out.has_value()) << second_out.error().message;

    Inflater first_inflater(source_capacity);
    Inflater second_inflater(source_capacity);
    const std::array<std::string, 2> writes = {"one write", "another write"};

    for (const std::string & payload : writes)
    {
        ASSERT_TRUE(first->write(ArrByteView(payload)));
        ASSERT_TRUE(second->write(ArrByteView(payload + " on second")));

        Result<ArrByte> first_inflated = first_inflater.inflate_stream(ArrByteView(view_of(*first_out)));
        Result<ArrByte> second_inflated = second_inflater.inflate_stream(ArrByteView(view_of(*second_out)));
        ASSERT_TRUE(first_inflated.has_value()) << first_inflated.error().message;
        ASSERT_TRUE(second_inflated.has_value()) << second_inflated.error().message;
        EXPECT_EQ(first_inflated->cpp_str_view(), payload);
        EXPECT_EQ(second_inflated->cpp_str_view(), payload + " on second");
    }

    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());
}

TEST_F(TestDevDeflate, test_dev_deflate_level_sets_the_compression)
{
    const std::string payload = text_of(source_capacity);
    Core stored_core("stored_core");
    Core best_core("best_core");
    DevDeflate *stored = this->wire(stored_core, "message");
    ASSERT_TRUE(stored->set_conf_int("level", Deflater::min_level));
    this->start(stored_core);
    ASSERT_TRUE(_source->write(ArrByteView(payload)));
    const size_t stored_size = _compressed->byte_size();
    EXPECT_EQ(view_of(_restored), payload);

    DevDeflate *best = this->wire(best_core, "message");
    ASSERT_TRUE(best->set_conf_int("level", Deflater::max_level));
    this->start(best_core);
    ASSERT_TRUE(_source->write(ArrByteView(payload)));
    const size_t best_size = _compressed->byte_size();
    EXPECT_EQ(view_of(_restored), payload);

    EXPECT_GT(stored_size, payload.size());
    EXPECT_LT(best_size, payload.size() / 10);
    EXPECT_TRUE(stored_core.stop());
    EXPECT_TRUE(best_core.stop());
}

TEST_F(TestDevDeflate, test_dev_deflate_gzip_format)
{
    Core core;
    DevDeflate *deflate = this->wire(core, "message");
    ASSERT_TRUE(deflate->set_conf_str("format", "gzip"));
    this->start(core);

    ASSERT_TRUE(_source->write(ArrByteView(std::string_view("gzip through channels"))));

    ASSERT_GE(_compressed->byte_size(), 2u);
    EXPECT_EQ(_compressed->data()[0], 0x1f);
    EXPECT_EQ(_compressed->data()[1], 0x8b);
    EXPECT_EQ(view_of(_restored), "gzip through channels");
    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());
}

TEST_F(TestDevDeflate, test_dev_deflate_full_capacity_noise_fits)
{
    Core core;
    this->wire(core, "stream");
    this->start(core);
    const std::string noise = noise_of(source_capacity);

    ASSERT_TRUE(_source->write(ArrByteView(noise)));

    EXPECT_GT(_compressed->byte_size(), noise.size());
    EXPECT_EQ(view_of(_restored), noise);
    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());
}

TEST_F(TestDevDeflate, test_dev_deflate_restart_starts_new_streams)
{
    Core core;
    this->wire(core, "stream");
    this->start(core);
    ASSERT_TRUE(_source->write(ArrByteView(std::string_view("before the restart"))));
    ASSERT_TRUE(core.stop());

    ASSERT_TRUE(core.start());
    ASSERT_TRUE(_source->write(ArrByteView(std::string_view("after the restart"))));

    EXPECT_EQ(view_of(_restored), "after the restart");
    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());
}

TEST_F(TestDevDeflate, test_dev_deflate_rejects_unknown_confs)
{
    DevDeflate deflate("deflate");

    EXPECT_FALSE(deflate.set_conf_str("mode", "flush").has_value());
    EXPECT_FALSE(deflate.set_conf_int("level", Deflater::max_level + 1).has_value());
    EXPECT_FALSE(deflate.set_conf_int("level", Deflater::min_level - 1).has_value());
    EXPECT_FALSE(deflate.set_conf_str("format", "zip").has_value());
}

} // namespace test
