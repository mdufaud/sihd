#include <sihd/sys/SigThreadBlocker.hpp>

#if !defined(__SIHD_WINDOWS__)

namespace sihd::sys
{

SigThreadBlocker::SigThreadBlocker(int sig): _blocking(false)
{
    const int sigs[] = {sig};
    this->_block(sigs);
}

SigThreadBlocker::SigThreadBlocker(std::span<const int> sigs): _blocking(false)
{
    this->_block(sigs);
}

SigThreadBlocker::SigThreadBlocker(std::initializer_list<int> sigs): _blocking(false)
{
    this->_block(std::span<const int>(sigs.begin(), sigs.size()));
}

SigThreadBlocker::~SigThreadBlocker()
{
    if (!_blocking)
        return;

    // drain any pending instances so restoring the mask does not deliver them
    struct timespec ts = {0, 0};
    while (sigtimedwait(&_blocked_set, nullptr, &ts) > 0)
        ;

    pthread_sigmask(SIG_SETMASK, &_old_set, nullptr);
}

void SigThreadBlocker::_block(std::span<const int> sigs)
{
    sigemptyset(&_blocked_set);
    for (int sig : sigs)
        sigaddset(&_blocked_set, sig);

    _blocking = pthread_sigmask(SIG_BLOCK, &_blocked_set, &_old_set) == 0;
}

} // namespace sihd::sys

#endif
