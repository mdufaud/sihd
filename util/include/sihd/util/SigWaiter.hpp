#ifndef __SIHD_UTIL_SIGWAITER_HPP__
#define __SIHD_UTIL_SIGWAITER_HPP__

#include <optional>

#include <sihd/util/Timestamp.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::util
{

class SigWaiter
{
    public:
        struct Conf
        {
                std::optional<int> signal = std::nullopt;
                Timestamp timeout = 0;
        };

        SigWaiter();
        explicit SigWaiter(const Conf & conf);
        ~SigWaiter();

        // Check if received a signal
        bool received_signal() const;

    private:
        bool _do_wait(int sig, Timestamp timeout);

        bool _received_signal;
};

} // namespace sihd::util

#endif
