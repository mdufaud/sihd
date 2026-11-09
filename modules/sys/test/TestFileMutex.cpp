#include <functional>
#include <iostream>
#include <shared_mutex>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/sys/FileMutex.hpp>
#include <sihd/sys/Process.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ansi.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/time.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <signal.h>
#endif

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;
class TestFileMutex: public ::testing::Test
{
    protected:
        TestFileMutex() { LoggerManager::stream(); }

        virtual ~TestFileMutex() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

#if !defined(__SIHD_WINDOWS__)
        // what a blocked waiter throws when a signal without SA_RESTART interrupts it
        static std::string interrupted_lock_error(const std::function<void(FileMutex &)> & lock)
        {
            TmpDir tmpdir;
            const std::string path = fs::combine(tmpdir.path(), "filemutex");
            FileMutex holder(path);
            FileMutex waiter(path);
            struct sigaction action {};
            action.sa_handler = +[](int) {
                                 };
            struct sigaction previous {};
            if (sigaction(SIGUSR1, &action, &previous) != 0)
                return "sigaction failed";
            Defer restore([&] { sigaction(SIGUSR1, &previous, nullptr); });
            holder.lock();

            std::string error;
            std::thread locker([&] {
                try
                {
                    lock(waiter);
                }
                catch (const std::exception & e)
                {
                    error = e.what();
                }
            });
            // nothing signals that flock blocks: give it time, a late signal only hides the bug
            time::msleep(100);
            pthread_kill(locker.native_handle(), SIGUSR1);
            time::msleep(50);
            holder.unlock();
            locker.join();
            return error;
        }
#endif
};

TEST_F(TestFileMutex, test_filemutex_guard)
{
    TmpDir tmpdir;
    File file;

    ASSERT_TRUE(file.open_tmp(tmpdir.path() + "/", false, "filemutex"));

    FileMutex mutex(std::move(file));
    SIHD_TRACE("Lock guard mutex: {}", mutex.file().path());

    std::lock_guard lock(mutex);
}

TEST_F(TestFileMutex, test_filemutex_unique)
{
    TmpDir tmpdir;

    File file;
    ASSERT_TRUE(file.open_tmp(tmpdir.path() + "/", false, "filemutex"));
    FileMutex mutex(std::move(file));
    SIHD_TRACE("Unique lock mutex: {}", mutex.file().path());

    std::unique_lock lock(mutex);
    EXPECT_TRUE(lock.owns_lock());
    ASSERT_NO_THROW(lock.unlock());
    ASSERT_TRUE(lock.try_lock());

#if !defined(__SIHD_WINDOWS__)
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
    (void)proc.execute();
    (void)proc.wait_any();

    EXPECT_TRUE(proc.return_code() & (1 << 1));
    EXPECT_TRUE(proc.return_code() & (1 << 2));
    EXPECT_TRUE(proc.return_code() & (1 << 3));
#else
    // windows: LockFileEx locks are per-handle, a second FileMutex on the same path conflicts
    FileMutex other(mutex.file().path());
    std::unique_lock other_lock(other, std::defer_lock);

    EXPECT_FALSE(other_lock.try_lock());
    EXPECT_FALSE(other_lock.try_lock_for(std::chrono::milliseconds(5)));
    EXPECT_FALSE(other_lock.try_lock_until(std::chrono::system_clock::now() + std::chrono::milliseconds(5)));
#endif
}

#if !defined(__SIHD_WINDOWS__)
TEST_F(TestFileMutex, test_filemutex_lock_survives_signal)
{
    if constexpr (build::is_emscripten)
        GTEST_SKIP() << "emscripten has no signal to interrupt flock";

    const std::string lock_error = interrupted_lock_error([](FileMutex & mutex) {
        mutex.lock();
        mutex.unlock();
    });
    const std::string lock_shared_error = interrupted_lock_error([](FileMutex & mutex) {
        mutex.lock_shared();
        mutex.unlock_shared();
    });

    EXPECT_EQ(lock_error, "");
    EXPECT_EQ(lock_shared_error, "");
}
#endif

TEST_F(TestFileMutex, test_filemutex_shared)
{
    TmpDir tmpdir;

    File file;
    ASSERT_TRUE(file.open_tmp(tmpdir.path() + "/filemutex_", false, ".mtx"));
    FileMutex mutex(std::move(file));
    SIHD_TRACE("Shared lock mutex: {}", mutex.file().path());

    std::shared_lock lock(mutex);

    EXPECT_TRUE(lock.owns_lock());
    ASSERT_NO_THROW(lock.unlock());
    ASSERT_TRUE(lock.try_lock());

#if !defined(__SIHD_WINDOWS__)
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
    (void)proc.execute();
    (void)proc.wait_any();

    EXPECT_TRUE(proc.return_code() & (1 << 1));
    EXPECT_TRUE(proc.return_code() & (1 << 2));
    EXPECT_TRUE(proc.return_code() & (1 << 3));
#else
    // windows: a second handle may share the lock while we hold it shared
    FileMutex other(mutex.file().path());
    std::shared_lock other_lock(other, std::defer_lock);

    EXPECT_TRUE(other_lock.try_lock());
    ASSERT_NO_THROW(other_lock.unlock());
    EXPECT_TRUE(other_lock.try_lock_for(std::chrono::milliseconds(5)));
    ASSERT_NO_THROW(other_lock.unlock());
    EXPECT_TRUE(other_lock.try_lock_until(std::chrono::system_clock::now() + std::chrono::milliseconds(5)));
#endif
}

} // namespace test
