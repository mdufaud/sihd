#ifndef __SIHD_SYS_SEMAPHORE_HPP__
#define __SIHD_SYS_SEMAPHORE_HPP__

#include <sys/stat.h> // mode_t

#include <string>
#include <string_view>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/build.hpp>

#if defined(__SIHD_EMSCRIPTEN__)
# define mode_t unsigned int
#endif

namespace sihd::sys
{

class Semaphore
{
    public:
        static constexpr bool supported = sihd::util::build::is_windows
                                          || (sihd::util::build::is_unix && !sihd::util::build::is_android
                                              && !sihd::util::build::is_apple && !sihd::util::build::is_emscripten);

        Semaphore();
        Semaphore(Semaphore && other);
        Semaphore & operator=(Semaphore && other);
        ~Semaphore();

        Semaphore(const Semaphore &) = delete;
        Semaphore & operator=(const Semaphore &) = delete;

        sihd::util::Result<void> create(std::string_view name, unsigned int value, mode_t mode = 0600);

        sihd::util::Result<void> attach(std::string_view name);

        sihd::util::Result<void> clear();

        bool wait();
        bool try_wait();
        bool wait_for(sihd::util::Duration duration);
        bool post();

        template <class Rep, class Period>
        bool wait_for(const std::chrono::duration<Rep, Period> & duration)
        {
            return this->wait_for(sihd::util::Duration(duration));
        }

        template <class Clock, class Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration> & timepoint)
        {
            return this->wait_for(sihd::util::Duration(sihd::util::Timestamp(timepoint).get()
                                                       - sihd::util::Timestamp(Clock::now()).get()));
        }

        sihd::util::Result<int> value() const;

        const std::string & name() const { return _name; }

        bool creator() const { return _created; }

    protected:

    private:
        void *_sem;
        bool _created;
        std::string _name;
};

} // namespace sihd::sys

#if defined(__SIHD_EMSCRIPTEN__)
# undef mode_t
#endif

#endif
