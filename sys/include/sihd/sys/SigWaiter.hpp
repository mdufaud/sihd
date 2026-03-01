#ifndef __SIHD_SYS_SIGWAITER_HPP__
#define __SIHD_SYS_SIGWAITER_HPP__

#include <optional>

#include <sihd/util/Timestamp.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::sys
{

class SigWaiter
{
    public:
        struct Conf
        {
                std::optional<int> signal = std::nullopt;
                sihd::util::Timestamp timeout = 0;
        };

        SigWaiter();
        explicit SigWaiter(const Conf & conf);
        ~SigWaiter();

        // Check if received a signal
        bool received_signal() const;

    private:
        bool _do_wait(int sig, sihd::util::Timestamp timeout);

        bool _received_signal;
};

} // namespace sihd::sys

#endif
