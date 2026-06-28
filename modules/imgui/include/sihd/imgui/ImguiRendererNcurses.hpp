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
    uint32_t ch; // Unicode codepoint (0 = empty)
    uint8_t  fg; // ANSI-256 foreground index
    uint8_t  bg; // ANSI-256 background index
};

class ImguiRendererNcurses : public sihd::imgui::IImguiRenderer
{
public:
    ImguiRendererNcurses();
    virtual ~ImguiRendererNcurses();

    bool init();

    void    new_frame() override;
    void    render(ImDrawData *draw_data) override;
    void    shutdown() override;
    void    resize() override;

    void    set_clear_color(ImVec4 *clear_color) override;
    ImVec4 *get_clear_color() override;

    // ── Pure functions (public for unit testing) ─────────────────────────────
    static uint8_t  rgb_to_ansi256(uint8_t r, uint8_t g, uint8_t b);
    static uint8_t  col_to_ansi256(ImU32 col);
    static uint8_t  col_to_ansi256_premul(ImU32 col);
    static uint32_t classify_arrow(ImVec2 p0, ImVec2 p1, ImVec2 p2);
    static bool     is_glyph_triangle(const ImDrawVert &v0, const ImDrawVert &v1, const ImDrawVert &v2);
    static ImVec4   compute_clip_rect(const ImDrawCmd &cmd, const ImVec2 &off, const ImVec2 &scale);
    static bool     is_cross_pattern(const ImVec2 pts[12]);

    // ── Headless test API ───────────────────────────────────────────────────
    bool           init_headless(int w, int h);
    const TCell &  get_cell(int x, int y) const;
    void           clear_screen();
    // Rasterise real ImDrawData into the cell buffer without flushing to ncurses.
    // Lets headless tests feed geometry built by a real ImGui frame.
    void           rasterize_headless(ImDrawData *draw_data);
    // Cache widget colours (CheckMark, SliderGrab) from the active ImGui style.
    // Called by _apply_terminal_style; exposed so headless tests can populate the
    // colour gates after creating a context + StyleColorsDark.
    void           cache_style_colors();

    // ── Draw methods (public for unit testing) ──────────────────────────────
    void draw_nonglyph_triangle(ImVec2 p0, ImVec2 p1, ImVec2 p2, ImU32 col, const ImVec4 &clip);
    void draw_rect(float xmin, float ymin, float xmax, float ymax, ImU32 col, const ImVec4 &clip);
    void draw_fill_triangle(ImVec2 p0, ImVec2 p1, ImVec2 p2, uint8_t bg, const ImVec4 &clip);

private:
    // ── Style ─────────────────────────────────────────────────────────────────
    // Port of ImTui_ImplText_Init() — call once after StyleColorsDark().
    void _apply_terminal_style();

    // ── Font atlas ────────────────────────────────────────────────────────────
    // Build the UV-to-codepoint reverse lookup from the font atlas glyphs.
    void     _build_uv_map();
    // Round float UV to nearest atlas texel to survive imgui's tiny float offsets.
    uint64_t _pack_uv(float u, float v) const;

    // ── Color conversion (delegates to public static methods) ──────────────

    // ── Bounds check ──────────────────────────────────────────────────────────
    bool _in_clip(int x, int y, const ImVec4 &clip) const;

    // ── Primitive rasterisers ─────────────────────────────────────────────────

    // Top-level entry: clear cell buffer + dispatch all draw lists.
    void _rasterize(ImDrawData *draw_data);
    // Process all draw commands for one ImDrawList.
    void _rasterize_draw_list(const ImDrawList *dl, int fb_w, int fb_h,
                              const ImVec2 &clip_off, const ImVec2 &clip_scale);
    // Handle one DrawCmd: optional callback, clip-rect guard, triangle loop.
    void _rasterize_cmd(const ImDrawList *dl, const ImDrawCmd &cmd,
                        int fb_w, int fb_h,
                        const ImVec2 &clip_off, const ImVec2 &clip_scale);

    // Cross detection: 4 consecutive non-glyph same-color triangles forming an X.
    // Returns extra element count consumed (9) or 0 if not a cross.
    unsigned int _try_detect_cross(const ImDrawList *dl, const ImDrawCmd &cmd,
                                   unsigned int i, const ImDrawVert &v0, const ImVec4 &clip);
    // Checkmark detection: 2 consecutive CheckMark-colour non-glyph triangles
    // (imgui RenderCheckMark polyline). Writes 'x' at the centroid cell, preserving
    // frame bg. Returns 3 on success, 0 otherwise.
    unsigned int _try_detect_check(const ImDrawList *dl, const ImDrawCmd &cmd,
                                   unsigned int i, const ImDrawVert &v0, const ImVec4 &clip);
    // Rect-pair detection: 2 consecutive same-color triangles forming axis-aligned rect.
    // Returns extra element count consumed (3) or 0 if not a rect pair.
    unsigned int _try_detect_rect_pair(const ImDrawList *dl, const ImDrawCmd &cmd,
                                       unsigned int i, const ImDrawVert &v0,
                                       const ImDrawVert &v1, const ImDrawVert &v2,
                                       const ImVec4 &clip);
    // Line-quad detection: 2 consecutive tris sharing 2 indices (AddLine pattern).
    // Skips arrow gate and fills both directly. Returns 3 on success, 0 otherwise.
    unsigned int _try_detect_line_quad(const ImDrawList *dl, const ImDrawCmd &cmd,
                                       unsigned int i, const ImDrawVert &v0,
                                       const ImDrawVert &v1, const ImDrawVert &v2,
                                       const ImVec4 &clip);

    // Text glyph: consume the two-triangle quad imgui emits per character.
    // tri1_base is the index of the first triangle's first element.
    // last_x/last_y deduplicate overlapping quads.
    void _draw_glyph_quad(const ImDrawVert &v0, const ImDrawVert &v1, const ImDrawVert &v2, const ImVec4 &clip);

    void _draw_rect(float xmin, float ymin, float xmax, float ymax,
                    ImU32 col, const ImVec4 &clip);
    void _draw_nonglyph_triangle(ImVec2 p0, ImVec2 p1, ImVec2 p2,
                                 ImU32 col, const ImVec4 &clip);
    void _draw_fill_triangle(ImVec2 p0, ImVec2 p1, ImVec2 p2, uint8_t bg, const ImVec4 &clip);

    // ── ncurses output ────────────────────────────────────────────────────────

    // Post-render: inject '+' at bottom-right corner of resizable top-level windows.
    // imtui patches strip resize grip from imgui — re-add as post-pass.
    void  _inject_resize_grips();

    // Differential screen update: only re-draws rows that changed.
    void  _flush_to_ncurses();
    // Write one row to stdscr, batching cells with the same color pair.
    void  _flush_row(int y);
    // Return (or allocate) the ncurses color pair id for a fg/bg combination.
    short _get_or_alloc_color_pair(uint8_t fg, uint8_t bg);

    // ── State ─────────────────────────────────────────────────────────────────
    bool    _is_init;
    bool    _needs_full_redraw;
    ImVec4 *_clear_color_ptr;

    int _tex_w; // font atlas width  (texels)
    int _tex_h; // font atlas height (texels)

    std::unordered_map<uint64_t, uint32_t> _uv_map; // UV key → Unicode codepoint

    int                _screen_w;
    int                _screen_h;
    std::vector<TCell> _screen;      // current frame buffer
    std::vector<TCell> _screen_prev; // previous frame (diff rendering)
    std::vector<int>   _scanline_range; // reused by _draw_fill_triangle
    std::vector<char>  _row_buf;        // reused by _flush_row

    std::vector<short> _pair_cache; // flat 65536 table, key = bg*256+fg, -1 = empty
    short _next_pair_id;

    ImU32 _check_col;             // packed ImGuiCol_CheckMark, captured in _apply_terminal_style
    ImU32 _slider_grab_col;       // packed ImGuiCol_SliderGrab
    ImU32 _slider_grab_col_active; // packed ImGuiCol_SliderGrabActive
    ImU32 _plot_histogram_col;     // packed ImGuiCol_PlotHistogram
};

} // namespace sihd::imgui

#endif // __SIHD_IMGUI_IMGUIRENDERERNCURSES_HPP__
