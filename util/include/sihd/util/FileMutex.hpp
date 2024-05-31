#ifndef __SIHD_UTIL_FILEMUTEX_HPP__
#define __SIHD_UTIL_FILEMUTEX_HPP__

#include <string_view>

#include <sihd/util/File.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class FileMutex
{
    public:
        FileMutex() = default;
        FileMutex(File && file);
        FileMutex(std::string_view path, bool create_file_if_not_exist = true);
        FileMutex(FileMutex && other);
        ~FileMutex() = default;

        FileMutex(const FileMutex &) = delete;
        FileMutex & operator=(const FileMutex &) = delete;

        FileMutex & operator=(FileMutex && other);

        void lock();
        bool try_lock();
        void unlock();

        void lock_shared();
        bool try_lock_shared();
        void unlock_shared();

        template <class Rep, class Period>
        bool try_lock_for(const std::chrono::duration<Rep, Period> & duration)
        {
            return this->try_lock_for(Timestamp(duration));
        }

        template <class Clock, class Duration>
        bool try_lock_until(const std::chrono::time_point<Clock, Duration> & timepoint)
        {
            return this->try_lock_for(Timestamp(timepoint) - Clock::now().time_since_epoch().count());
        }

        bool try_lock_for(Timestamp duration);

        template <class Rep, class Period>
        bool try_lock_shared_for(const std::chrono::duration<Rep, Period> & duration)
        {
            return this->try_lock_shared_for(Timestamp(duration));
        }

        template <class Clock, class Duration>
        bool try_lock_shared_until(const std::chrono::time_point<Clock, Duration> & timepoint)
        {
            return this->try_lock_shared_for(Timestamp(timepoint) - Clock::now().time_since_epoch().count());
        }

        bool try_lock_shared_for(Timestamp duration);

        File & file();

    protected:

    private:
        File _file;
};

} // namespace sihd::util

#endif
