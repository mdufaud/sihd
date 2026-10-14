#ifndef __SIHD_SYS_WAYLAND_OUTPUTS_HPP__
#define __SIHD_SYS_WAYLAND_OUTPUTS_HPP__

// Shared wl_output binding of the wayland backends: every output global bound,
// their announced state collected during the connection roundtrips.

#include <string>
#include <string_view>
#include <vector>
#include <wayland-client.h>

namespace sihd::sys::internal::wayland
{

struct OutputState
{
        wl_output *proxy = nullptr;
        std::string name;
        std::string model;
        int x = 0;
        int y = 0;
        int width_mm = 0;
        int height_mm = 0;
        int mode_width = 0;
        int mode_height = 0;
        int scale = 1;
};

class Outputs
{
    public:
        void bind(wl_registry *registry, uint32_t name, const char *interface, uint32_t version);

        const std::vector<OutputState> & states() const;

        // The logical layout extents of every announced output; valid is false
        // when no output announced a mode.
        struct Extents
        {
                bool valid = false;
                int x = 0;
                int y = 0;
                int width = 0;
                int height = 0;
        };

        Extents extents() const;

        // The output whose announced name (v4) is `name`.
        wl_output *find(std::string_view name) const;

    private:
        OutputState *find_state(wl_output *proxy);

        const wl_output_listener _listener = {
            .geometry =
                [](void *data,
                   wl_output *proxy,
                   int32_t x,
                   int32_t y,
                   int32_t width_mm,
                   int32_t height_mm,
                   int32_t,
                   const char *,
                   const char *model,
                   int32_t) {
                    OutputState *state = static_cast<Outputs *>(data)->find_state(proxy);
                    if (state == nullptr)
                        return;
                    state->x = x;
                    state->y = y;
                    state->width_mm = width_mm;
                    state->height_mm = height_mm;
                    state->model = model;
                },
            .mode =
                [](void *data, wl_output *proxy, uint32_t flags, int32_t width, int32_t height, int32_t) {
                    if ((flags & WL_OUTPUT_MODE_CURRENT) == 0)
                        return;
                    OutputState *state = static_cast<Outputs *>(data)->find_state(proxy);
                    if (state == nullptr)
                        return;
                    state->mode_width = width;
                    state->mode_height = height;
                },
            .done = [](void *, wl_output *) {},
            .scale =
                [](void *data, wl_output *proxy, int32_t scale) {
                    OutputState *state = static_cast<Outputs *>(data)->find_state(proxy);
                    if (state != nullptr)
                        state->scale = scale;
                },
            .name =
                [](void *data, wl_output *proxy, const char *name) {
                    OutputState *state = static_cast<Outputs *>(data)->find_state(proxy);
                    if (state != nullptr)
                        state->name = name;
                },
            .description = [](void *, wl_output *, const char *) {},
        };

        std::vector<OutputState> _states;
};

} // namespace sihd::sys::internal::wayland

#endif
