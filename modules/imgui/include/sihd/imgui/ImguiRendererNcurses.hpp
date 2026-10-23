#ifndef __SIHD_IMGUI_IMGUIRENDERERNCURSES_HPP__
#define __SIHD_IMGUI_IMGUIRENDERERNCURSES_HPP__

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sihd/imgui/IImguiRenderer.hpp>

namespace sihd::imgui
{

struct TCell
{
        uint32_t ch = 0;
        uint32_t fg = 0;
        uint32_t bg = 0;
        bool has_bg = false;
        bool dim = false;
        bool reverse = false;
        bool underline = false;
        bool wide_tail = false;

        bool operator==(const TCell & other) const = default;
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
        static uint32_t classify_arrow(ImVec2 p0, ImVec2 p1, ImVec2 p2);
        static ImVec4 compute_clip_rect(const ImDrawCmd & cmd, const ImVec2 & off, const ImVec2 & scale);
        static constexpr size_t cross_pattern_vertex_count = 12;
        static bool is_cross_pattern(const ImVec2 pts[cross_pattern_vertex_count]);

        // needs a current ImGui context: applies the same font and style as init(), without ncurses
        bool init_headless(int w, int h);
        void set_unicode(bool unicode);
        const TCell & get_cell(int x, int y) const;
        void clear_screen();
        void rasterize_headless(ImDrawData *draw_data);

    private:
        enum class Role
        {
            fill,
            line,
            separator,
            table_line,
            check,
            grip,
            text_mark,
            plot_line,
            histogram,
            nav,
            reverse,
            dim,
            underline,
            link_underline,
            grab,
            tab,
            scrollbar,
            scrollbar_grab,
            hidden,
        };

        struct Primitive
        {
                unsigned int first = 0;
                unsigned int end = 0;
                ImU32 col = 0;
                ImVec2 min;
                ImVec2 max;
                unsigned int first_vertex = 0;
                unsigned int vertex_count = 0;
                bool gradient = false;
        };

        struct TableBorderRow
        {
                int x0 = 0;
                int x1 = 0;
                int y = 0;
        };

        struct TableBorderTop
        {
                int x = 0;
                int y = 0;
                ImU32 col = 0;
        };

        void _setup_imgui();
        // call once after StyleColorsDark()
        void _apply_terminal_style();
        void _tag_style_colors();
        void _tag_link_underlines();
        Role _role_of(ImU32 col) const;

        void _ack_textures(ImDrawData *draw_data) const;
        void _rasterize(ImDrawData *draw_data);
        void _rasterize_list(const ImDrawList *dl, const ImDrawData & draw_data, const ImVec2 & fb_size);
        void _rasterize_cmd(const ImDrawList *dl, const ImDrawCmd & cmd, const ImVec4 & clip);
        Primitive _collect_primitive(const ImDrawList *dl, const ImDrawCmd & cmd, unsigned int first) const;
        std::optional<ImU32>
            _sample_primitive(const ImDrawList *dl, const ImDrawCmd & cmd, const Primitive & prim, int x, int y) const;

        void _draw_image(const Primitive & prim, const ImVec4 & clip);
        void _draw_glyph(const ImDrawVert & top_left, const ImDrawVert & top_right, const ImVec4 & clip);
        void _draw_fill(const ImDrawList *dl, const ImDrawCmd & cmd, const Primitive & prim, const ImVec4 & clip);
        void _draw_shaded_fill(const ImDrawList *dl,
                               const ImDrawCmd & cmd,
                               const Primitive & prim,
                               uint32_t ch,
                               ImU32 ch_col,
                               const ImVec4 & clip);
        void _paint_cell(int x, int y, ImU32 col, const ImVec4 & clip);
        void _draw_grab(const ImDrawList *dl, const ImDrawCmd & cmd, const Primitive & prim, const ImVec4 & clip);
        void _draw_tab(const ImDrawList *dl, const ImDrawCmd & cmd, const Primitive & prim, const ImVec4 & clip);
        void _draw_histogram(const ImDrawList *dl, const ImDrawCmd & cmd, const Primitive & prim, const ImVec4 & clip);
        void _draw_progress_bar(const Primitive & prim, const ImVec4 & clip);
        void _draw_histogram_bar(const Primitive & prim, const ImVec4 & clip);
        void _put_block(int x,
                        int y,
                        uint8_t eighths,
                        const std::array<uint32_t, 8> & blocks,
                        ImU32 col,
                        const ImVec4 & clip);
        void _draw_line(const ImDrawList *dl,
                        const ImDrawCmd & cmd,
                        const Primitive & prim,
                        bool table,
                        const ImVec4 & clip);
        std::optional<bool>
            _stroke_is_vertical(const ImDrawList *dl, const ImDrawCmd & cmd, const Primitive & prim) const;
        void _join_table_borders(const ImVec4 & clip);
        void _draw_separator(const ImDrawList *dl, const ImDrawCmd & cmd, const Primitive & prim, const ImVec4 & clip);
        float _ring_thickness(const ImDrawList *dl, const ImDrawCmd & cmd, const Primitive & prim) const;
        void _add_horizontal_line(int x0, int x1, int y, ImU32 col, const ImVec4 & clip);
        void _add_vertical_line(int x, int y0, int y1, ImU32 col, const ImVec4 & clip);
        void _draw_mark(int x, int y, uint32_t ch, ImU32 col, const ImVec4 & clip);
        unsigned int
            _draw_text_mark(const ImDrawList *dl, const ImDrawCmd & cmd, const Primitive & first, const ImVec4 & clip);
        bool _draw_mark_run(const ImDrawList *dl, const ImDrawCmd & cmd, const ImVec4 & clip);
        uint32_t _mark_char(const ImVec2 & size) const;
        uint32_t _arrow_char(uint32_t ascii_arrow) const;
        uint32_t _pick(uint32_t unicode_ch, uint32_t ascii_ch) const;
        bool _is_untextured(const ImDrawVert & vertex) const;
        void _draw_plot_line(const ImDrawList *dl, const ImDrawCmd & cmd, const Primitive & prim, const ImVec4 & clip);
        void _set_attribute(const Primitive & prim, Role role, const ImVec4 & clip);

        void _add_line_arms(int x, int y, uint8_t arms, ImU32 col, const ImVec4 & clip);
        void _add_braille_dot(float x, float y, ImU32 col, const ImVec4 & clip);
        bool _in_clip(int x, int y, const ImVec4 & clip) const;
        TCell & _cell(int x, int y);
        uint32_t _blend(uint32_t under, ImU32 col) const;
        uint32_t _cell_bg(const TCell & cell) const;
        uint32_t _box_char(uint8_t arms) const;

        void _resize_grid(int w, int h);
        size_t _index(int x, int y) const;

        void _flush_to_ncurses();
        void _flush_row(int y);
        wchar_t _cell_char(int x, int y) const;
        int _color_index(uint32_t rgb) const;

        bool _is_init;
        bool _unicode;
        int _max_colors;
        ImVec4 *_clear_color_ptr;
        ImVec2 _white_uv;

        std::unordered_map<uint32_t, int> _rgb_roles;
        std::array<ImVec4, ImGuiCol_COUNT> _tagged_colors;

        int _screen_w;
        int _screen_h;
        std::vector<TCell> _screen;
        // last grid written to stdscr: an identical frame leaves stdscr untouched, which the backend reads as idle
        std::vector<TCell> _flushed;
        std::vector<uint8_t> _line_arms;
        std::vector<uint8_t> _braille;
        std::vector<wchar_t> _row_buf;
        std::vector<Primitive> _mark_run;
        std::vector<ImVec2> _mark_points;
        std::vector<std::pair<Primitive, ImVec4>> _deferred_nav;
        std::vector<TableBorderRow> _table_border_rows;
        std::vector<TableBorderTop> _table_border_tops;
};

} // namespace sihd::imgui

#endif // __SIHD_IMGUI_IMGUIRENDERERNCURSES_HPP__
