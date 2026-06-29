#include <sihd/sys/SigThreadBlocker.hpp>
#include <sihd/sys/signal.hpp>

namespace sihd::sys
{

SigThreadBlocker::SigThreadBlocker(int sig): _sig(sig), _blocking(signal::block_thread(sig)) {}

SigThreadBlocker::~SigThreadBlocker()
{
    if (_blocking)
        signal::unblock_thread(_sig);
}

} // namespace sihd::sys
