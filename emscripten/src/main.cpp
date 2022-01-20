#include <sihd/util/Logger.hpp>

NEW_LOGGER("emscripten")

using namespace sihd::util;

int main()
{
    LoggerManager::basic(stdout);
    LOG(info, "hello world");
    return (0);
}