#include <sihd/bt/Advertiser.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::bt
{

using namespace sihd::util;

SIHD_REGISTER_FACTORY(Advertiser)

bool Advertiser::set_adapter(size_t index)
{
    _adapter_index = index;
    return true;
}

bool Advertiser::set_advertisement(const Advertisement & advertisement)
{
    if (this->is_running())
        return false;
    _advertisement = advertisement;
    return true;
}

} // namespace sihd::bt
