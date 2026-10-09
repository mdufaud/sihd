#include <stdexcept>

#include <sihd/sys/LoggerSystem.hpp>
#include <sihd/util/NamedFactory.hpp>

using namespace sihd::util;

namespace sihd::sys
{

LoggerSystem::LoggerSystem(const std::string & name, Node *parent): ALogger(name, parent), _progname(name)
{
    this->add_conf("facility", &LoggerSystem::set_facility);
    auto opened = this->_open_source();
    if (!opened)
        throw std::runtime_error(opened.error().message);
}

LoggerSystem::LoggerSystem(std::string_view progname, int facility, int options):
    ALogger("system"),
    _progname(progname),
    _facility(facility),
    _options(options)
{
    auto opened = this->_open_source();
    if (!opened)
        throw std::runtime_error(opened.error().message);
}

bool LoggerSystem::set_facility(int facility)
{
    _facility = facility;
    return true;
}

SIHD_REGISTER_FACTORY(LoggerSystem);

} // namespace sihd::sys
