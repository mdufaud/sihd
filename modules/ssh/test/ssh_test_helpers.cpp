#include "ssh_test_helpers.hpp"

namespace test
{

SIHD_LOGGER;

bool connect_to_test_server(const SshServerHelper & server,
                            SshSession & session,
                            const char *user,
                            const char *password)
{
    // process_config=false: ignore ~/.ssh/config (proxy) for localhost tests
    if (SIHD_UNEXPECTED_LOG(
            session.fast_connect({.user = user, .host = "127.0.0.1", .port = server.port, .process_config = false})))
        return false;
    if (!session.connected())
        return false;
    auto auth = session.auth_password(password);
    return auth.success();
}

} // namespace test
