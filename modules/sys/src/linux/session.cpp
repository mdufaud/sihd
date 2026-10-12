#include <sihd/sys/platform.hpp>

#if !defined(__SIHD_ANDROID__)
# include <utmpx.h>
#endif

#include <cstring> // strnlen

#include <sihd/sys/session.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;

namespace sihd::sys::session
{

#if defined(__SIHD_ANDROID__)
std::vector<Session> sessions()
{
    // no utmp database on bionic
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
        session.since = Timestamp(time::sec((time::UnixTime)entry->ut_tv.tv_sec)
                                  + (time::UnixTime)entry->ut_tv.tv_usec * 1000);
        session.id = (uint32_t)entry->ut_pid;
        session.state = SessionState::active;
        ret.push_back(std::move(session));
    }
    endutxent();
    return ret;
}

#endif

} // namespace sihd::sys::session
