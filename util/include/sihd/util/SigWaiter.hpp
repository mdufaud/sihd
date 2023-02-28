#ifndef __SIHD_UTIL_SIGWAITER_HPP__
#define __SIHD_UTIL_SIGWAITER_HPP__

#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class SigWaiter
{
    public:
        struct Conf
        {
                int sig = -1;
                Timestamp polling_frequency = std::chrono::milliseconds(2);
                Timestamp timeout = 0;
        };

    public:
        SigWaiter();
        SigWaiter(const Conf & conf);
        ~SigWaiter();

        bool received_signal() const;

    protected:

    private:
        int _sig;
        bool _received_signal;
};

} // namespace sihd::util

#endif
