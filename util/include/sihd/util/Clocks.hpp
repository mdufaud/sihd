#ifndef __SIHD_UTIL_CLOCKS_HPP__
# define __SIHD_UTIL_CLOCKS_HPP__

# include <chrono>

namespace sihd::util
{

class IClock
{
    public:
        virtual ~IClock() {};
        virtual std::time_t now() = 0;
        virtual bool is_steady() = 0;
        virtual bool start() = 0;
        virtual bool stop() = 0;
};

class SteadyClock:  public IClock
{
    public:
        SteadyClock();
        ~SteadyClock();

        virtual std::time_t now();
        virtual bool is_steady();
        virtual bool start() { return true; };
        virtual bool stop() { return true; };

    private:
        std::chrono::steady_clock   _clock;
};

class SystemClock:  public IClock
{
    public:
        SystemClock();
        ~SystemClock();

        virtual std::time_t now();
        virtual bool is_steady();
        virtual bool start() { return true; };
        virtual bool stop() { return true; };

    private:
        std::chrono::system_clock   _clock;
};

class Clock
{
    private:
        Clock();
        ~Clock();

    public:
        static SystemClock default_clock;
};

}

#endif