#include <stdexcept>

#include <sihd/sys/DynLib.hpp>

namespace sihd::sys
{

DynLib::DynLib(): _handle(nullptr) {}

DynLib::DynLib(const std::string & file): DynLib()
{
    auto opened = this->open(file);
    if (!opened)
        throw std::runtime_error(opened.error().message);
}

DynLib::~DynLib()
{
    this->close();
}

} // namespace sihd::sys
