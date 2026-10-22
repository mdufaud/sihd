#include <sihd/core/ACoreObject.hpp>

namespace sihd::core
{

using namespace sihd::util;

ACoreObject::ACoreObject(const std::string & name, Node *parent): Named(name, parent) {}

bool ACoreObject::is_running() const
{
    return false;
}

void ACoreObject::handle(Channel *) {}

} // namespace sihd::core
