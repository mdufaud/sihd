#include <cmath>

#include <gtest/gtest.h>

#include <sihd/sys/fs.hpp>
#include <sihd/sys/info.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;
class TestInfo: public ::testing::Test
{
    protected:
        TestInfo() { sihd::util::LoggerManager::stream(); }

        virtual ~TestInfo() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestInfo, test_info_memory)
{
#if defined(__SIHD_EMSCRIPTEN__)
    GTEST_SKIP() << "no /proc/meminfo on emscripten";
#else
    const auto total = info::total_ram();
    ASSERT_TRUE(total.has_value());
    EXPECT_GT(*total, 0u);
    const auto available = info::available_ram();
    ASSERT_TRUE(available.has_value());
    EXPECT_LE(*available, *total);
    SIHD_LOG(debug, "RAM: {} / {} bytes", *available, *total);

    const auto total_swap = info::total_swap();
    ASSERT_TRUE(total_swap.has_value());
    const auto free_swap = info::free_swap();
    ASSERT_TRUE(free_swap.has_value());
    EXPECT_LE(*free_swap, *total_swap);
#endif
}

TEST_F(TestInfo, test_info_cpu_count)
{
    EXPECT_GE(info::cpu_count(), 1u);
    const auto physical = info::physical_core_count();
    if (physical.has_value())
    {
        EXPECT_GE(*physical, 1u);
        EXPECT_LE(*physical, info::cpu_count());
    }
    SIHD_LOG(debug, "cores: {} physical / {} logical", physical.value_or(0), info::cpu_count());
}

TEST_F(TestInfo, test_info_cpu_times)
{
#if defined(__SIHD_EMSCRIPTEN__)
    GTEST_SKIP() << "no /proc/stat on emscripten";
#else
    const auto times = info::cpu_times();
    ASSERT_TRUE(times.has_value());
    EXPECT_LE(times->busy(), times->total());

    const auto per_core = info::per_core_cpu_times();
    // per-core counters are unavailable on some systems (win32 stubs under wine)
    if (!per_core.empty())
    {
        EXPECT_EQ(per_core.size(), (size_t)info::cpu_count());
        for (const auto & core_times : per_core)
        {
            EXPECT_LE(core_times.busy(), core_times.total());
        }
    }

    const auto frequency = info::cpu_frequency();
    if (frequency.has_value())
    {
        EXPECT_GT(*frequency, 0u);
    }
    const auto max_frequency = info::cpu_max_frequency();
    if (max_frequency.has_value())
    {
        EXPECT_GT(*max_frequency, 0u);
    }
#endif
}

TEST_F(TestInfo, test_info_cpu_usage)
{
#if defined(__SIHD_EMSCRIPTEN__)
    GTEST_SKIP() << "no /proc/stat on emscripten";
#else
    info::CpuUsage usage;
    EXPECT_DOUBLE_EQ(usage.usage(), -1.0);
    EXPECT_TRUE(usage.per_core_usage().empty());

    ASSERT_TRUE(usage.sample());
    EXPECT_GE(usage.usage(), 0.0);
    EXPECT_LE(usage.usage(), 100.0);
    if (!usage.per_core_usage().empty())
    {
        EXPECT_EQ(usage.per_core_usage().size(), (size_t)info::cpu_count());
    }
    for (double core_usage : usage.per_core_usage())
    {
        EXPECT_GE(core_usage, 0.0);
        EXPECT_LE(core_usage, 100.0);
    }

    time::msleep(100);
    ASSERT_TRUE(usage.sample());
    EXPECT_GE(usage.usage(), 0.0);
    EXPECT_LE(usage.usage(), 100.0);
    SIHD_LOG(debug, "cpu usage: {:.1f}%", usage.usage());
#endif
}

TEST_F(TestInfo, test_info_system)
{
    EXPECT_FALSE(info::hostname().empty());
#if !defined(__SIHD_EMSCRIPTEN__)
    EXPECT_GT(info::uptime().nanoseconds(), 0);
#endif
    const auto load = info::load_average();
    for (double average : load)
    {
        EXPECT_GE(average, 0.0);
        EXPECT_TRUE(std::isfinite(average));
    }
    SIHD_LOG(debug,
             "load: {:.2f} {:.2f} {:.2f}, domain: {}",
             load[0],
             load[1],
             load[2],
             info::domain().value_or("(none)"));
}

TEST_F(TestInfo, test_info_os)
{
    const info::OsInfo os = info::os_info();
    SIHD_LOG(debug, "os: {} {} (kernel {}, arch {})", os.name, os.version, os.kernel, os.arch);
    EXPECT_FALSE(os.kernel.empty());
    EXPECT_FALSE(os.arch.empty());
}

TEST_F(TestInfo, test_info_hardware_identity)
{
    const info::HardwareIdentity id = info::hardware_identity();
    SIHD_LOG(debug, "hardware: {} {} (serial '{}')", id.vendor, id.model, id.serial);
    // fields are best effort: no DMI on emscripten/embedded, serial is root-only on linux
#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__) && !defined(__SIHD_ANDROID__)
    if (fs::exists("/sys/class/dmi/id/product_name"))
    {
        EXPECT_FALSE(id.model.empty());
    }
#endif
}

} // namespace test
