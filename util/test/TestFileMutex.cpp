#include <iostream>
#include <shared_mutex>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

#include <sihd/util/FileMutex.hpp>
#include <sihd/util/Process.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::util;
class TestFileMutex: public ::testing::Test
{
    protected:
        TestFileMutex() { sihd::util::LoggerManager::basic(); }

        virtual ~TestFileMutex() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestFileMutex, test_filemutex_guard)
{
    TmpDir tmpdir;
    File file;

    ASSERT_TRUE(file.open_tmp(tmpdir.path() + "/", false, "filemutex"));

    FileMutex mutex(std::move(file));
    SIHD_TRACE("Lock guard mutex: {}", mutex.file().path());

    std::lock_guard l(mutex);
}
TEST_F(TestFileMutex, test_filemutex_unique)
{
    TmpDir tmpdir;

    File file;
    ASSERT_TRUE(file.open_tmp(tmpdir.path() + "/", false, "filemutex"));
    FileMutex mutex(std::move(file));
    SIHD_TRACE("Unique lock mutex: {}", mutex.file().path());

    std::unique_lock l(mutex);
    EXPECT_TRUE(l.owns_lock());
    ASSERT_NO_THROW(l.unlock());
    ASSERT_TRUE(l.try_lock());

    Process proc;
    proc.set_function([path = mutex.file().path()] {
        FileMutex mutex(path);
        std::unique_lock other_lock(mutex, std::defer_lock);
        int ret = 0;

        if (other_lock.try_lock() == false)
            ret += 1 << 1;

        if (other_lock.try_lock_for(std::chrono::milliseconds(5)) == false)
            ret += 1 << 2;

        if (other_lock.try_lock_until(std::chrono::system_clock::now() + std::chrono::milliseconds(5)) == false)
            ret += 1 << 3;

        return ret;
    });
    proc.execute();
    proc.wait_any();

    EXPECT_TRUE(proc.return_code() & (1 << 1));
    EXPECT_TRUE(proc.return_code() & (1 << 2));
    EXPECT_TRUE(proc.return_code() & (1 << 3));
}

TEST_F(TestFileMutex, test_filemutex_shared)
{
    TmpDir tmpdir;

    File file;
    ASSERT_TRUE(file.open_tmp(tmpdir.path() + "/filemutex_", false, ".mtx"));
    FileMutex mutex(std::move(file));
    SIHD_TRACE("Shared lock mutex: {}", mutex.file().path());

    std::shared_lock l(mutex);

    EXPECT_TRUE(l.owns_lock());
    ASSERT_NO_THROW(l.unlock());
    ASSERT_TRUE(l.try_lock());

    Process proc;
    proc.set_function([path = mutex.file().path()] {
        FileMutex mutex(path);
        std::shared_lock other_lock(mutex, std::defer_lock);
        int ret = 0;

        if (other_lock.try_lock())
        {
            ret += 1 << 1;
            other_lock.unlock();
        }

        if (other_lock.try_lock_for(std::chrono::milliseconds(5)))
        {
            ret += 1 << 2;
            other_lock.unlock();
        }

        if (other_lock.try_lock_until(std::chrono::system_clock::now() + std::chrono::milliseconds(5)))
            ret += 1 << 3;

        return ret;
    });
    proc.execute();
    proc.wait_any();

    EXPECT_TRUE(proc.return_code() & (1 << 1));
    EXPECT_TRUE(proc.return_code() & (1 << 2));
    EXPECT_TRUE(proc.return_code() & (1 << 3));
}

} // namespace test
