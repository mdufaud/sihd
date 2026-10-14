#include "outputs.hpp"

#include <algorithm>
#include <cstring>

namespace sihd::sys::internal::wayland
{

OutputState *Outputs::find_state(wl_output *proxy)
{
    for (OutputState & state : _states)
    {
        if (state.proxy == proxy)
            return &state;
    }
    return nullptr;
}

void Outputs::bind(wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    if (strcmp(interface, wl_output_interface.name) != 0)
        return;
    // Version 4 announces the output name, scale arrives from version 2.
    wl_output *bound = static_cast<wl_output *>(
        wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 4u)));
    if (bound == nullptr)
        return;
    OutputState & state = _states.emplace_back();
    state.proxy = bound;
    wl_output_add_listener(bound, &_listener, this);
}

const std::vector<OutputState> & Outputs::states() const
{
    return _states;
}

Outputs::Extents Outputs::extents() const
{
    Extents out;
    int min_x = 0;
    int min_y = 0;
    int max_right = 0;
    int max_bottom = 0;
    for (const OutputState & output : _states)
    {
        if (output.mode_width == 0 || output.mode_height == 0)
            continue;
        // The mode is in device pixels, the layout is logical.
        const int right = output.x + output.mode_width / output.scale;
        const int bottom = output.y + output.mode_height / output.scale;
        if (!out.valid)
        {
            min_x = output.x;
            min_y = output.y;
            max_right = right;
            max_bottom = bottom;
            out.valid = true;
        }
        else
        {
            min_x = std::min(min_x, output.x);
            min_y = std::min(min_y, output.y);
            max_right = std::max(max_right, right);
            max_bottom = std::max(max_bottom, bottom);
        }
    }
    out.x = min_x;
    out.y = min_y;
    out.width = max_right - min_x;
    out.height = max_bottom - min_y;
    return out;
}

wl_output *Outputs::find(std::string_view name) const
{
    for (const OutputState & state : _states)
    {
        if (state.name == name)
            return state.proxy;
    }
    return nullptr;
}

} // namespace sihd::sys::internal::wayland
