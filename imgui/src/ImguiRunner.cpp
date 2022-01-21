#include <sihd/imgui/ImguiRunner.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::imgui
{

SIHD_UTIL_REGISTER_FACTORY(ImguiRunner)

LOGGER;

ImguiRunner::ImguiRunner(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

ImguiRunner::~ImguiRunner()
{
}

}