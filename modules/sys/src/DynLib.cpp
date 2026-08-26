#include <sihd/sys/DynLib.hpp>

namespace sihd::sys
{

DynLib::DynLib(): _handle(nullptr) {}

DynLib::DynLib(std::string_view lib_name): DynLib()
{
    this->open(lib_name);
}

DynLib::~DynLib()
{
    this->close();
}

} // namespace sihd::sys
