#include <sihd/util/Logger.hpp>

SIHD_NEW_LOGGER("emscripten")

using namespace sihd::util;

int main()
{
    LoggerManager::basic(stdout);
    SIHD_LOG(info, "hello world");
    return (0);
}