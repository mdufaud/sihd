#include <sihd/sys/DynLib.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::sys
{

SIHD_LOGGER;

DynLib::DynLib(): _handle(nullptr) {}

DynLib::DynLib(std::string_view lib_name): DynLib()
{
    SIHD_UNEXPECTED_LOG(this->open(lib_name));
}

DynLib::~DynLib()
{
    this->close();
}

} // namespace sihd::sys
