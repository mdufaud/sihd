// clang-format off
# include <windows.h>
# include <wtsapi32.h>
// clang-format on

#include <string>
#include <vector>

#include <sihd/sys/session.hpp>

namespace sihd::sys::session
{

namespace
{

std::string wide_to_utf8(const wchar_t *wide)
{
    if (wide == nullptr)
        return "";
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1)
        return "";
    std::string out(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), size, nullptr, nullptr);
    return out;
}

SessionState parse_state(WTS_CONNECTSTATE_CLASS state)
{
    switch (state)
    {
        case WTSActive:
            return SessionState::active;
        case WTSConnected:
            return SessionState::connected;
        case WTSDisconnected:
            return SessionState::disconnected;
        default:
            return SessionState::unknown;
    }
}

} // namespace

std::vector<Session> sessions()
{
    std::vector<Session> ret;
    WTS_SESSION_INFOW *infos = nullptr;
    ULONG count = 0;
    if (WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &infos, &count) == 0)
        return ret;
    for (ULONG i = 0; i < count; ++i)
    {
        const WTS_SESSION_INFOW & info = infos[i];
        Session session;

        // no logon time exposed by WTS
        session.id = (uint32_t)info.SessionId;
        session.device = wide_to_utf8(info.pWinStationName);
        session.state = parse_state(info.State);
        LPWSTR buffer = nullptr;
        DWORD bytes = 0;

        if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, info.SessionId, WTSUserName, &buffer, &bytes) != 0)
        {
            session.username = wide_to_utf8(buffer);
            WTSFreeMemory(buffer);
        }
        // the WTSCLIENT strings are ANSI regardless of the entry point
        CHAR *client_buffer = nullptr;
        DWORD client_bytes = 0;
        if (WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE,
                                        info.SessionId,
                                        WTSClientInfo,
                                        &client_buffer,
                                        &client_bytes)
            != 0)
        {
            // ClientName is the remote client machine name, empty for local sessions
            session.hostname = ((const WTSCLIENTA *)client_buffer)->ClientName;
            WTSFreeMemory(client_buffer);
        }
        ret.push_back(std::move(session));
    }
    WTSFreeMemory(infos);
    return ret;
}

} // namespace sihd::sys::session
