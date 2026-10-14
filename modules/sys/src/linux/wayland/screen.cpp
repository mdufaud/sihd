#include "screen.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <sihd/sys/screen.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "../../screen_math.hpp"
#include "../internal/deadline.hpp"
#include "connection.hpp"
#include "outputs.hpp"

// The announced state of every wl_output: layout position, current mode,
// scale factor and name

using enum sihd::util::ErrorCode;

namespace sihd::sys::screen::wayland
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::screen");

namespace
{

constexpr Duration list_timeout = Duration(time::sec(1));

class Connection: public sihd::sys::internal::wayland::Connection
{
    public:
        explicit Connection(Timestamp deadline): sihd::sys::internal::wayland::Connection() { bind_globals(deadline); }

        sihd::sys::internal::wayland::Outputs & outputs() { return _outputs; }

    protected:
        void bind_global(wl_registry *registry, uint32_t name, const char *interface, uint32_t version) override
        {
            _outputs.bind(registry, name, interface, version);
        }

        bool ready() const override { return true; }

    private:
        sihd::sys::internal::wayland::Outputs _outputs;
};

Monitor assemble(const sihd::sys::internal::wayland::OutputState & output, bool primary)
{
    Monitor monitor;
    monitor.name = !output.name.empty() ? output.name : output.model;
    monitor.x = output.x;
    monitor.y = output.y;
    // The mode is in device pixels, the desktop space is logical.
    monitor.width = output.mode_width / output.scale;
    monitor.height = output.mode_height / output.scale;
    monitor.width_mm = output.width_mm;
    monitor.height_mm = output.height_mm;
    monitor.scale = output.scale;
    monitor.dpi = internal::dpi_from(output.mode_width, output.width_mm);
    monitor.primary = primary;
    return monitor;
}

} // namespace

Result<std::vector<Monitor>> monitors()
{
    const Timestamp deadline = sihd::sys::internal::deadline_from_now(list_timeout);
    Connection conn(deadline);
    if (!conn.ok())
        return std::unexpected(Error(io_error, "could not connect to the display"));

    std::vector<Monitor> monitors;
    for (const sihd::sys::internal::wayland::OutputState & output : conn.outputs().states())
    {
        if (output.mode_width == 0 || output.mode_height == 0)
            continue;
        // Wayland exposes no primary output: the first announced one stands in.
        monitors.push_back(assemble(output, monitors.empty()));
    }
    return monitors;
}

} // namespace sihd::sys::screen::wayland
