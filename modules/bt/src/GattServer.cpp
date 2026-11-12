#include <sihd/bt/GattServer.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::bt
{

using namespace sihd::util;

SIHD_REGISTER_FACTORY(GattServer)

bool GattServer::set_adapter(size_t index)
{
    _adapter_index = index;
    return true;
}

bool GattServer::add_service(const LocalService & service)
{
    if (this->is_running())
        return false;
    _services.push_back(service);
    return true;
}

} // namespace sihd::bt
