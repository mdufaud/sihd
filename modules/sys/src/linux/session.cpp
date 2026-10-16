#include <sihd/sys/platform.hpp>

#if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# include <utmpx.h>
#endif

#include <cstring> // strnlen

#include <sihd/sys/session.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;

namespace sihd::sys::session
{

#if defined(__SIHD_ANDROID__) || defined(__SIHD_EMSCRIPTEN__)
std::vector<Session> sessions()
{
    return {};
}
#else

namespace
{

std::string fixed_string(const char *buf, size_t size)
{
    return std::string(buf, strnlen(buf, size));
}

} // namespace

std::vector<Session> sessions()
{
    std::vector<Session> ret;
    setutxent();
    while (const utmpx *entry = getutxent())
    {
        if (entry->ut_type != USER_PROCESS)
            continue;
        Session session;
        session.username = fixed_string(entry->ut_user, sizeof(entry->ut_user));
        session.device = fixed_string(entry->ut_line, sizeof(entry->ut_line));
        session.hostname = fixed_string(entry->ut_host, sizeof(entry->ut_host));
        const timeval since_tv {static_cast<time_t>(entry->ut_tv.tv_sec),
                                static_cast<suseconds_t>(entry->ut_tv.tv_usec)};
        session.since = Timestamp(time::tv(since_tv));
        session.id = (uint32_t)entry->ut_pid;
        session.state = SessionState::active;
        ret.push_back(std::move(session));
    }
    endutxent();
    return ret;
}

#endif

} // namespace sihd::sys::session
