#ifndef __SIHD_SYS_SESSION_HPP__
#define __SIHD_SYS_SESSION_HPP__

#include <cstdint>
#include <string>
#include <vector>

#include <sihd/util/Timestamp.hpp>

namespace sihd::sys::session
{

enum class SessionState
{
    active,
    connected,
    disconnected,
    unknown,
};

struct Session
{
        std::string username;
        // tty / terminal / workstation name
        std::string device;
        // empty for local sessions
        std::string hostname;
        // epoch 0 when unknown
        sihd::util::Timestamp since {0};
        uint32_t id = 0;
        SessionState state = SessionState::unknown;
};

std::vector<Session> sessions();

} // namespace sihd::sys::session

#endif
