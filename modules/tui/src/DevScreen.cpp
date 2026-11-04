#include <sihd/tui/DevScreen.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::tui
{

SIHD_REGISTER_FACTORY(DevScreen)

SIHD_NEW_LOGGER("sihd::tui");

using namespace sihd::util;
using namespace sihd::core;

DevScreen::DevScreen(const std::string & name, Node *parent): Device(name, parent), _running(false), _refresher(nullptr)
{
}

DevScreen::~DevScreen() = default;

bool DevScreen::is_running() const
{
    return _running;
}

void DevScreen::set_refresher(Refresher *refresher)
{
    _refresher = refresher;
}

void DevScreen::handle(Channel *)
{
    _refresher->request();
}

bool DevScreen::on_start()
{
    if (_refresher == nullptr)
    {
        SIHD_LOG(error, "DevScreen: {} has no refresher", this->name());
        return false;
    }
    auto inputs = this->linked_channels();
    if (SIHD_UNEXPECTED_LOG(inputs))
        return false;
    for (const auto & [name, channel] : *inputs)
    {
        if (this->observe_channel(channel) == false)
            return false;
    }
    _running = true;
    return true;
}

bool DevScreen::on_stop()
{
    _running = false;
    return true;
}

} // namespace sihd::tui
