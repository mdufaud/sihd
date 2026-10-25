#include <stdexcept>

#include <sihd/sys/DynLib.hpp>

namespace sihd::sys
{

using namespace sihd::util;

DynLib::DynLib(): _handle(nullptr) {}

DynLib::DynLib(ZStringView file): DynLib()
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
