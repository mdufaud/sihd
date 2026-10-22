#ifndef __SIHD_IMGUI_IMGUIRENDERERNCURSES_HPP__
#define __SIHD_IMGUI_IMGUIRENDERERNCURSES_HPP__

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <sihd/imgui/IImguiRenderer.hpp>

namespace sihd::imgui
{

struct TCell
{
        uint32_t ch; // 0 = empty
        uint8_t fg;
        uint8_t bg;
};

class ImguiRendererNcurses: public sihd::imgui::IImguiRenderer
{
    public:
        ImguiRendererNcurses();
        virtual ~ImguiRendererNcurses();

        bool init();

        void new_frame() override;
        void render(ImDrawData *draw_data) override;
        void shutdown() override;
        void resize() override;

        void set_clear_color(ImVec4 *clear_color) override;
        ImVec4 *get_clear_color() override;

        static uint8_t rgb_to_ansi256(uint8_t r, uint8_t g, uint8_t b);
        static uint8_t col_to_ansi256(ImU32 col);
        static uint8_t col_to_ansi256_premul(ImU32 col);
        static uint32_t classify_arrow(ImVec2 p0, ImVec2 p1, ImVec2 p2);
        static bool is_glyph_triangle(const ImDrawVert & v0, const ImDrawVert & v1, const ImDrawVert & v2);
        static ImVec4 compute_clip_rect(const ImDrawCmd & cmd, const ImVec2 & off, const ImVec2 & scale);
        static constexpr size_t cross_pattern_vertex_count = 12;
        static bool is_cross_pattern(const ImVec2 pts[cross_pattern_vertex_count]);

        bool init_headless(int w, int h);
        const TCell & get_cell(int x, int y) const;
        void clear_screen();
        void rasterize_headless(ImDrawData *draw_data);
        // public so headless tests can populate the colour gates without _apply_terminal_style
        void cache_style_colors();

        void draw_nonglyph_triangle(ImVec2 p0, ImVec2 p1, ImVec2 p2, ImU32 col, const ImVec4 & clip);
        void draw_rect(float xmin, float ymin, float xmax, float ymax, ImU32 col, const ImVec4 & clip);
        void draw_fill_triangle(ImVec2 p0, ImVec2 p1, ImVec2 p2, uint8_t bg, const ImVec4 & clip);

    private:
        // call once after StyleColorsDark()
        void _apply_terminal_style();

        void _build_uv_map();
        uint64_t _pack_uv(float u, float v) const;

        bool _in_clip(int x, int y, const ImVec4 & clip) const;

        void _rasterize(ImDrawData *draw_data);
        void _rasterize_draw_list(const ImDrawList *dl,
                                  int fb_w,
                                  int fb_h,
                                  const ImVec2 & clip_off,
                                  const ImVec2 & clip_scale);
        void _rasterize_cmd(const ImDrawList *dl,
                            const ImDrawCmd & cmd,
                            int fb_w,
                            int fb_h,
                            const ImVec2 & clip_off,
                            const ImVec2 & clip_scale);

        // returns extra elements consumed (9) or 0
        unsigned int _try_detect_cross(const ImDrawList *dl,
                                       const ImDrawCmd & cmd,
                                       unsigned int i,
                                       const ImDrawVert & v0,
                                       const ImVec4 & clip);
        // returns extra elements consumed (3) or 0
        unsigned int _try_detect_check(const ImDrawList *dl,
                                       const ImDrawCmd & cmd,
                                       unsigned int i,
                                       const ImDrawVert & v0,
                                       const ImVec4 & clip);
        // returns extra elements consumed (3) or 0
        unsigned int _try_detect_rect_pair(const ImDrawList *dl,
                                           const ImDrawCmd & cmd,
                                           unsigned int i,
                                           const ImDrawVert & v0,
                                           const ImDrawVert & v1,
                                           const ImDrawVert & v2,
                                           const ImVec4 & clip);
        // returns extra elements consumed (3) or 0
        unsigned int _try_detect_line_quad(const ImDrawList *dl,
                                           const ImDrawCmd & cmd,
                                           unsigned int i,
                                           const ImDrawVert & v0,
                                           const ImDrawVert & v1,
                                           const ImDrawVert & v2,
                                           const ImVec4 & clip);

        void _draw_glyph_quad(const ImDrawVert & v0, const ImDrawVert & v1, const ImDrawVert & v2, const ImVec4 & clip);

        void _draw_rect(float xmin, float ymin, float xmax, float ymax, ImU32 col, const ImVec4 & clip);
        void _draw_nonglyph_triangle(ImVec2 p0, ImVec2 p1, ImVec2 p2, ImU32 col, const ImVec4 & clip);
        void _draw_fill_triangle(ImVec2 p0, ImVec2 p1, ImVec2 p2, uint8_t bg, const ImVec4 & clip);

        // imtui patches strip the resize grip from imgui: re-add it as a post-pass
        void _inject_resize_grips();
        void _clear_grip_block(int cx, int cy, int grip_x, int grip_y);

        void _flush_to_ncurses();
        void _flush_row(int y);
        short _get_or_alloc_color_pair(uint8_t fg, uint8_t bg);

        bool _is_init;
        bool _needs_full_redraw;
        ImVec4 *_clear_color_ptr;

        int _tex_w;
        int _tex_h;

        std::unordered_map<uint64_t, uint32_t> _uv_map;

        int _screen_w;
        int _screen_h;
        std::vector<TCell> _screen;
        std::vector<TCell> _screen_prev;
        std::vector<int> _scanline_range;
        std::vector<char> _row_buf;

        std::vector<short> _pair_cache; // key = bg*256+fg, -1 = empty
        short _next_pair_id;

        ImU32 _check_col;
        ImU32 _slider_grab_col;
        ImU32 _slider_grab_col_active;
        ImU32 _plot_histogram_col;
};

} // namespace sihd::imgui

#endif // __SIHD_IMGUI_IMGUIRENDERERNCURSES_HPP__
