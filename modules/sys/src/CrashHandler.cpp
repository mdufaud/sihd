#include <sihd/sys/CrashHandler.hpp>

namespace sihd::sys
{

CrashHandler::CrashHandler(const Options & options)
{
    install(options);
}

CrashHandler::~CrashHandler()
{
    uninstall();
}

} // namespace sihd::sys
