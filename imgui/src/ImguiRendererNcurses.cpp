#include <ncurses.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <imgui_internal.h>

#include <sihd/imgui/ImguiRendererNcurses.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::imgui
{

SIHD_LOGGER;

namespace
{

constexpr float kSubCellThreshold  = 0.5f;
constexpr float kCrossMaxSpan      = 3.0f;
constexpr float kVertexEpsilon     = 0.01f;
constexpr uint8_t kMinVisibleAlpha = 8;

// ── Small helper utilities ──────────────────────────────────────────────────

inline ImVec2 tri_centroid(ImVec2 a, ImVec2 b, ImVec2 c)
{
    return {(a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f};
}

// Env-driven trace match. SIHD_IMGUI_TRACE_CELL="x,y" — fires WRITE trace at (cx,cy).
inline bool trace_cell(int cx, int cy)
{
    static int tx = -2, ty = -2;
    static bool parsed = false;
    if (!parsed)
    {
        parsed = true;
        const char *env = std::getenv("SIHD_IMGUI_TRACE_CELL");
        if (env && *env)
        {
            const char *comma = std::strchr(env, ',');
            if (comma)
            {
                tx = std::atoi(env);
                ty = std::atoi(comma + 1);
            }
        }
    }
    return cx == tx && cy == ty;
}

// Env-gated CSV dump of every triangle entering the classifier.
// Set SIHD_IMGUI_NCURSES_DUMP=/path/to/dump.csv to enable.
// SIHD_IMGUI_NCURSES_DUMP_FRAMES=N caps frames (default 1).
struct DumpState
{
    FILE *fp = nullptr;
    int   frame = 0;
    int   cl = -1;
    int   cmd = -1;
    int   max_frames = 1;
    bool  inited = false;
};

inline DumpState & dump_state()
{
    static DumpState s;
    return s;
}

inline void dump_init_if_needed()
{
    DumpState & s = dump_state();
    if (s.inited)
        return;
    s.inited = true;
    const char *path = std::getenv("SIHD_IMGUI_NCURSES_DUMP");
    if (!path || !*path)
        return;
    const char *n = std::getenv("SIHD_IMGUI_NCURSES_DUMP_FRAMES");
    if (n && *n)
        s.max_frames = std::max(1, std::atoi(n));
    s.fp = std::fopen(path, "w");
    if (s.fp)
    {
        std::fprintf(s.fp,
                     "frame,cl,cmd,tri,col,clipx,clipy,clipz,clipw,"
                     "p0x,p0y,p1x,p1y,p2x,p2y,u0,v0,u1,v1,u2,v2\n");
        std::fflush(s.fp);
    }
}

inline void dump_frame_begin()
{
    DumpState & s = dump_state();
    dump_init_if_needed();
    if (!s.fp)
        return;
    s.frame++;
    s.cl = -1;
    s.cmd = -1;
    if (s.frame > s.max_frames)
    {
        std::fclose(s.fp);
        s.fp = nullptr;
    }
}

inline void dump_cl_begin(int cl)
{
    dump_state().cl = cl;
    dump_state().cmd = -1;
}

inline void dump_cmd_begin(int cmd)
{
    dump_state().cmd = cmd;
}

inline bool dump_active()
{
    return dump_state().fp != nullptr;
}

inline void dump_triangle(int cl, int cmd, unsigned int tri, const ImVec4 & clip,
                          const ImDrawVert & v0, const ImDrawVert & v1, const ImDrawVert & v2)
{
    DumpState & s = dump_state();
    if (!s.fp)
        return;
    std::fprintf(s.fp,
                 "%d,%d,%d,%u,%08x,%.3f,%.3f,%.3f,%.3f,"
                 "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                 s.frame, cl, cmd, tri, v0.col,
                 clip.x, clip.y, clip.z, clip.w,
                 v0.pos.x, v0.pos.y, v1.pos.x, v1.pos.y, v2.pos.x, v2.pos.y,
                 v0.uv.x, v0.uv.y, v1.uv.x, v1.uv.y, v2.uv.x, v2.uv.y);
}

} // namespace

// ── Scanline fill (ported verbatim from imtui-impl-text.cpp) ─────────────────

// Bresenham walk along one triangle edge.
// Updates xr[2*y] (min-x) and xr[2*y+1] (max-x) per scanline row.
static void scanline_extend(int x1, int y1, int x2, int y2, int ymax, std::vector<int> & xr)
{
    int sx = x2 - x1, sy = y2 - y1;
    int dx1 = (sx > 0) ? 1 : (sx < 0) ? -1 : 0;
    int dy1 = (sy > 0) ? 1 : (sy < 0) ? -1 : 0;
    int m = std::abs(sx), n = std::abs(sy);
    int dx2 = dx1, dy2 = 0;
    if (m < n)
    {
        m = std::abs(sy);
        n = std::abs(sx);
        dx2 = 0;
        dy2 = dy1;
    }

    int x = x1, y = y1;
    int cnt = m + 1, k = n / 2;
    while (cnt--)
    {
        if (y >= 0 && y < ymax)
        {
            if (x < xr[2 * y])
                xr[2 * y] = x;
            if (x > xr[2 * y + 1])
                xr[2 * y + 1] = x;
        }
        k += n;
        if (k < m)
        {
            x += dx2;
            y += dy2;
        }
        else
        {
            k -= m;
            x += dx1;
            y += dy1;
        }
    }
}


// ── Public static helpers (testable without ncurses) ────────────────────────

bool ImguiRendererNcurses::is_glyph_triangle(const ImDrawVert & v0, const ImDrawVert & v1, const ImDrawVert & v2)
{
    return v0.uv.x != v1.uv.x || v0.uv.x != v2.uv.x || v0.uv.y != v1.uv.y || v0.uv.y != v2.uv.y;
}

ImVec4 ImguiRendererNcurses::compute_clip_rect(const ImDrawCmd & cmd, const ImVec2 & off, const ImVec2 & scale)
{
    return {(cmd.ClipRect.x - off.x) * scale.x,
            (cmd.ClipRect.y - off.y) * scale.y,
            (cmd.ClipRect.z - off.x) * scale.x,
            (cmd.ClipRect.w - off.y) * scale.y};
}

// ── Color helpers (imtui rgbToAnsi256 formulas) ───────────────────────────────

// Pixel-coord key: round float UV to the nearest atlas texel to survive the
// tiny fractional offset imgui adds in its render path (≈0.04/tex_width).
uint64_t ImguiRendererNcurses::_pack_uv(float u, float v) const
{
    const uint32_t pu = static_cast<uint32_t>(std::max(0L, std::lroundf(u * static_cast<float>(_tex_w))));
    const uint32_t pv = static_cast<uint32_t>(std::max(0L, std::lroundf(v * static_cast<float>(_tex_h))));
    return (uint64_t)pu | ((uint64_t)pv << 32);
}

bool ImguiRendererNcurses::_in_clip(int x, int y, const ImVec4 & clip) const
{
    return x >= (int)clip.x && x < (int)clip.z && y >= (int)clip.y && y < (int)clip.w && x >= 0 && x < _screen_w
           && y >= 0 && y < _screen_h;
}

// Foreground color: ignore alpha (imtui: doAlpha=false for glyphs).
uint8_t ImguiRendererNcurses::col_to_ansi256(ImU32 col)
{
    return rgb_to_ansi256((col) & 0xFF, (col >> 8) & 0xFF, (col >> 16) & 0xFF);
}

// Background color: alpha-premultiplied (imtui: doAlpha=true for fills).
uint8_t ImguiRendererNcurses::col_to_ansi256_premul(ImU32 col)
{
    uint8_t r = (col) & 0xFF;
    uint8_t g = (col >> 8) & 0xFF;
    uint8_t b = (col >> 16) & 0xFF;
    const uint8_t a = (col >> 24) & 0xFF;
    const float scale = (float)a / 255.0f;
    return rgb_to_ansi256((uint8_t)std::roundf(r * scale),
                           (uint8_t)std::roundf(g * scale),
                           (uint8_t)std::roundf(b * scale));
}

// Core formula: grayscale ramp (232–255) for r==g==b, 6×6×6 cube otherwise.
uint8_t ImguiRendererNcurses::rgb_to_ansi256(uint8_t r, uint8_t g, uint8_t b)
{
    if (r == g && g == b)
    {
        if (r < 8)
            return 16;
        if (r > 248)
            return 231;
        return static_cast<uint8_t>(232 + std::lroundf(static_cast<float>(r - 8) / 247.0f * 24.0f));
    }
    const long ri = std::lroundf(static_cast<float>(r) / 255.0f * 5.0f);
    const long gi = std::lroundf(static_cast<float>(g) / 255.0f * 5.0f);
    const long bi = std::lroundf(static_cast<float>(b) / 255.0f * 5.0f);
    return static_cast<uint8_t>(16 + 36 * ri + 6 * gi + bi);
}

// ── Constructor / destructor ─────────────────────────────────────────────────

ImguiRendererNcurses::ImguiRendererNcurses():
    _is_init(false),
    _needs_full_redraw(false),
    _diag_enabled(false),
    _clear_color_ptr(nullptr),
    _tex_w(0),
    _tex_h(0),
    _screen_w(0),
    _screen_h(0),
    _pair_cache(65536, -1),
    _next_pair_id(1),
    _check_col(0),
    _slider_grab_col(0),
    _slider_grab_col_active(0),
    _plot_histogram_col(0),
    _diag {}
{
}

ImguiRendererNcurses::~ImguiRendererNcurses()
{
    this->shutdown();
}

void ImguiRendererNcurses::set_clear_color(ImVec4 *clear_color)
{
    _clear_color_ptr = clear_color;
}

ImVec4 *ImguiRendererNcurses::get_clear_color()
{
    return _clear_color_ptr;
}

// ── init() ───────────────────────────────────────────────────────────────────

bool ImguiRendererNcurses::init()
{
    if (_is_init)
        return true;

    _diag = {};
    const char *diag_env = std::getenv("SIHD_IMGUI_NCURSES_DIAG");
    _diag_enabled = diag_env != nullptr && diag_env[0] != '\0' && std::strcmp(diag_env, "0") != 0;
    if (_diag_enabled)
        SIHD_LOG(info, "ImguiRendererNcurses: diagnostics enabled via SIHD_IMGUI_NCURSES_DIAG");

    if (_clear_color_ptr == nullptr)
    {
        SIHD_LOG(error, "ImguiRendererNcurses: set_clear_color() must be called before init()");
        return false;
    }

    // Font: 1 unit = 1 terminal cell (matching imtui fontConfig)
    ImGuiIO & io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.SizePixels = 1.0f;
    cfg.GlyphMinAdvanceX = 1.0f;
    io.Fonts->AddFontDefault(&cfg);

    // Build atlas and retrieve pixel data (populates glyph UV coordinates)
    unsigned char *tex_pixels = nullptr;
    io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &_tex_w, &_tex_h);
    SIHD_LOG(info, "ImguiRendererNcurses: font atlas {}x{}", _tex_w, _tex_h);

    // Style — port of ImTui_ImplText_Init()
    _apply_terminal_style();

    // Build UV to codepoint reverse-lookup
    _build_uv_map();

    // Initialise ncurses colour support
    start_color();
    use_default_colors();

    _screen_w = COLS;
    _screen_h = LINES;
    const size_t n = (size_t)(_screen_w * _screen_h);
    _screen.assign(n, TCell {0, 0, 0});
    // Sentinel: differs from any real cell → forces a full redraw on the first frame
    _screen_prev.assign(n, TCell {(uint32_t)-1, 0xFFu, 0xFFu});

    _is_init = true;
    return true;
}

// ── _apply_terminal_style() ───────────────────────────────────────────────────
// Port of ImTui_ImplText_Init() style block. Call AFTER ImGui::StyleColorsDark().
//
// COLOR POLICY: ImTui only overrides 4 colors explicitly. All other colors,
// including TitleBgActive (blue) and Header/HeaderHovered (blue), are kept at
// their StyleColorsDark() defaults. That is exactly what makes the active window
// title and hovered rows blue in ImTui — do NOT override them here.

void ImguiRendererNcurses::_apply_terminal_style()
{
    ImGuiIO & io = ImGui::GetIO();
    ImGuiStyle & s = ImGui::GetStyle();

    // Geometry: all rounded/padded values collapse to 0 for single-cell rendering.
    s.Alpha = 1.0f;
    s.WindowPadding = ImVec2(0.5f, 0.0f);
    // DO NOT change WindowRounding — any value > 0 breaks resize grip '+' and other geometry
    s.WindowRounding = 0.0f;
    s.WindowBorderSize = 0.0f; // border=1 shifts ClipRect and clips the first char
    s.WindowMinSize = ImVec2(4.0f, 2.0f);
    s.WindowTitleAlign = ImVec2(0.0f, 0.0f);
    s.WindowMenuButtonPosition = ImGuiDir_Left;
    s.ChildRounding = 0.0f;
    s.ChildBorderSize = 0.0f;
    s.PopupRounding = 0.0f;
    s.PopupBorderSize = 0.0f;
    s.FramePadding = ImVec2(1.0f, 0.0f);
    s.FrameRounding = 0.0f;
    s.FrameBorderSize = 0.0f;
    s.ItemSpacing = ImVec2(1.0f, 0.0f);
    s.ItemInnerSpacing = ImVec2(1.0f, 0.0f);
    s.TouchExtraPadding = ImVec2(0.5f, 0.0f);
    s.IndentSpacing = 1.0f;
    s.ColumnsMinSpacing = 1.0f;
    s.ScrollbarSize = 0.5f;
    s.ScrollbarRounding = 0.0f;
    s.GrabMinSize = 0.1f;
    s.GrabRounding = 0.0f;
    s.TabRounding = 0.0f;
    s.TabBorderSize = 0.0f;
    s.TabBarBorderSize = 0.0f;
    s.TabBarOverlineSize = 0.0f;
    s.LogSliderDeadzone = 0.0f;
    s.ColorButtonPosition = ImGuiDir_Right;
    s.ButtonTextAlign = ImVec2(0.5f, 0.0f);
    s.SelectableTextAlign = ImVec2(0.0f, 0.0f);
    s.DisplayWindowPadding = ImVec2(0.0f, 0.0f);
    s.DisplaySafeAreaPadding = ImVec2(0.0f, 0.0f);
    s.CellPadding = ImVec2(1.0f, 0.0f);
    s.MouseCursorScale = 1.0f;
    s.AntiAliasedLines = false; // no AA geometry in terminal
    s.AntiAliasedFill = false;
    s.AntiAliasedLinesUseTex = false;
    s.CurveTessellationTol = 1.25f;
    s.CircleTessellationMaxError = 5.0f;
    io.MouseDragThreshold = 1.0f; // 1 cell = 1 unit
    io.ConfigWindowsResizeFromEdges = false;

    // ── Colors ────────────────────────────────────────────────────────────────
    // Exactly the 4 overrides ImTui makes; nothing else.
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    s.Colors[ImGuiCol_TitleBg] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    s.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    s.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.75f, 0.75f, 0.75f, 0.50f);

    s.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.70f, 0.70f, 0.70f, 0.60f);
    s.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.90f, 0.90f, 0.90f, 0.80f);
    s.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.95f);

    // NavHighlight draws a border rect that produces visual noise; suppress it.
    s.Colors[ImGuiCol_NavHighlight] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Resize border hover: 1.91.9 draws a 2-unit-thick highlight line with
    // SeparatorHovered — at terminal scale this is a huge colored bar.
    s.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    s.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Default is 4.0f (meant for pixel-density displays). At our scale
    // (1 font-unit = 1 cell), 4 cells of hover padding above every window
    // captures clicks meant for other widgets (e.g., main menu bar).
    s.WindowBorderHoverPadding = 0.5f;

    // SeparatorText: defaults (BorderSize=3, Padding=(20,3)) are for pixel
    // rendering. At terminal scale they produce 7-cell-tall separators with
    // 40 cells of horizontal padding — breaks popup menu layout completely.
    s.SeparatorTextBorderSize = 1.0f;
    s.SeparatorTextPadding = ImVec2(1.0f, 0.0f);
    s.SeparatorTextAlign = ImVec2(0.0f, 0.5f);

    cache_style_colors();
}

void ImguiRendererNcurses::cache_style_colors()
{
    // Cache CheckMark colour so the checkbox tick (2-triangle polyline) can be
    // detected and rendered as a single 'x' glyph instead of a coverage blob.
    _check_col = ImGui::GetColorU32(ImGuiCol_CheckMark);

    // Cache slider grab colours so the grab handle (a rect of arbitrary width) is
    // rendered as a single 'I' instead of a coverage block, matching the thin-grab
    // case for sliders with a wide handle (int/enum).
    _slider_grab_col = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    _slider_grab_col_active = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);

    // Cache plot histogram colour so a 1px-wide bar (high sample count clamps bar
    // width below a cell) is rendered as a 1-cell column instead of being dropped
    // by the sub-cell vertical sliver skip.
    _plot_histogram_col = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
}

void ImguiRendererNcurses::_build_uv_map()
{
    _uv_map.clear();
    const ImFontAtlas *atlas = ImGui::GetIO().Fonts;
    const bool trace = std::getenv("SIHD_IMGUI_TRACE_GLYPH") != nullptr;
    for (ImFont *font : atlas->Fonts)
    {
        for (const ImFontGlyph & g : font->Glyphs)
        {
            if (!g.Visible)
                continue;
            // Populate every texel in the glyph's atlas footprint, not just U0.
            // ImGui can clip text quads at fractional cell boundaries (e.g. tree
            // text region starts at x=8.5), which yields vertices whose UV falls
            // inside [U0,U1] rather than exactly at U0. A single-key map misses
            // those.
            const long pu0 = std::lroundf(g.U0 * (float)_tex_w);
            const long pu1 = std::lroundf(g.U1 * (float)_tex_w);
            const long pv0 = std::lroundf(g.V0 * (float)_tex_h);
            const long pv1 = std::lroundf(g.V1 * (float)_tex_h);
            for (long pv = pv0; pv <= pv1; ++pv)
            {
                for (long pu = pu0; pu <= pu1; ++pu)
                {
                    const uint64_t key = (uint64_t)(uint32_t)pu | ((uint64_t)(uint32_t)pv << 32);
                    _uv_map.emplace(key, g.Codepoint);
                }
            }
            (void)trace;
        }
    }
    SIHD_LOG(info,
             "ImguiRendererNcurses: UV map built, {} glyphs (fonts: {})",
             _uv_map.size(),
             ImGui::GetIO().Fonts->Fonts.Size);
}

// ── Per-frame hooks ───────────────────────────────────────────────────────────

void ImguiRendererNcurses::new_frame() {}

void ImguiRendererNcurses::resize()
{
    _screen_w = COLS;
    _screen_h = LINES;
    const size_t n = (size_t)(_screen_w * _screen_h);
    _screen.assign(n, TCell {0, 0, 0});
    _screen_prev.assign(n, TCell {0, 0, 0});
    _needs_full_redraw = true;
}

void ImguiRendererNcurses::render(ImDrawData *draw_data)
{
    if (_screen_w != COLS || _screen_h != LINES)
        resize();

    _rasterize(draw_data);
    _inject_resize_grips();
    _flush_to_ncurses();

    static int dump_frame = 0;
    static const char *dump_path = std::getenv("SIHD_IMGUI_NCURSES_SCREENDUMP");
    static const char *dump_frame_env = std::getenv("SIHD_IMGUI_NCURSES_DUMPFRAME");
    static const int target_frame = dump_frame_env ? std::atoi(dump_frame_env) : 2;
    if (dump_path && dump_frame == target_frame)
    {
        FILE *f = std::fopen(dump_path, "w");
        FILE *fbg = std::fopen((std::string(dump_path) + ".bg").c_str(), "w");
        if (f)
        {
            for (int y = 0; y < _screen_h; ++y)
            {
                for (int x = 0; x < _screen_w; ++x)
                {
                    const TCell &c = _screen[y * _screen_w + x];
                    char ch = (c.ch == 0 || c.ch > 126) ? '.' : (char)c.ch;
                    std::fputc(ch, f);
                }
                std::fputc('\n', f);
            }
            std::fclose(f);
        }
        if (fbg)
        {
            for (int y = 0; y < _screen_h; ++y)
            {
                for (int x = 0; x < _screen_w; ++x)
                {
                    const TCell &c = _screen[y * _screen_w + x];
                    std::fprintf(fbg, "%3u ", (unsigned)c.bg);
                }
                std::fputc('\n', fbg);
            }
            std::fclose(fbg);
        }
    }
    ++dump_frame;
}

void ImguiRendererNcurses::_inject_resize_grips()
{
    ImGuiContext *ctx = ImGui::GetCurrentContext();
    if (ctx == nullptr)
        return;
    for (int i = 0; i < ctx->Windows.Size; ++i)
    {
        ImGuiWindow *w = ctx->Windows[i];
        if (w == nullptr || !w->WasActive || w->Hidden)
            continue;
        if (w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_Popup))
            continue;
        const int gx = (int)std::floor(w->Pos.x + w->Size.x);
        const int gy = (int)std::floor(w->Pos.y + w->Size.y);

        // Skip grip if a higher-z window covers the grip cell. ctx->Windows is
        // back-to-front display order, so windows after i are drawn on top.
        const ImVec2 grip_pt((float)gx + 0.5f, (float)gy + 0.5f);
        bool occluded = false;
        for (int j = i + 1; j < ctx->Windows.Size && !occluded; ++j)
        {
            ImGuiWindow *top = ctx->Windows[j];
            if (top == nullptr || !top->WasActive || top->Hidden)
                continue;
            if (top->Flags & ImGuiWindowFlags_ChildWindow)
                continue;
            if (top->Rect().Contains(grip_pt))
                occluded = true;
        }
        if (occluded)
            continue;

        // Erase resize-grip triangle fill (stock imgui emits PathFillConvex at corner).
        // Triangle covers 1-2 cells just inside the bottom-right corner: (gx-1, gy-1).
        // Restore by copying bg from a clean neighbor 3 cells left.
        auto clear_block = [&](int cx, int cy) {
            if (cx < 0 || cx >= _screen_w || cy < 0 || cy >= _screen_h)
                return;
            TCell &dst = _screen[(size_t)(cy * _screen_w + cx)];
            // Skip if another widget/window already painted text here (occluded window).
            if (dst.ch != 0 && dst.ch != (uint32_t)' ')
                return;
            // Inside window bg region → restore window bg (copy clean neighbor).
            // Outside (grip triangle bled past window edge) → restore terminal bg.
            const bool inside = (cx < gx && cy < gy);
            if (trace_cell(cx, cy))
                std::fprintf(stderr, "WRITE (%d,%d) ch=' ' fn=resize_clear prev_ch=%u prev_bg=%u inside=%d win=%s pos=(%.1f,%.1f) size=(%.1f,%.1f) gx=%d gy=%d\n",
                             cx, cy, dst.ch, dst.bg, inside, w->Name, w->Pos.x, w->Pos.y, w->Size.x, w->Size.y, gx, gy);
            dst.ch = (uint32_t)' ';
            if (inside)
            {
                const int nx = (cx >= 3) ? (cx - 3) : 0;
                const TCell &src = _screen[(size_t)(cy * _screen_w + nx)];
                dst.fg = src.fg;
                dst.bg = src.bg;
            }
            else
            {
                dst.ch = 0;
                dst.fg = 0;
                dst.bg = 0;
            }
        };
        clear_block(gx - 1, gy - 1);
        clear_block(gx - 2, gy - 1);
        clear_block(gx - 1, gy - 2);
        clear_block(gx, gy - 1);

        // Place '+' at actual hit-test cell (one beyond visible window edge).
        if (gx < 0 || gx >= _screen_w || gy < 0 || gy >= _screen_h)
            continue;
        TCell &cell = _screen[(size_t)(gy * _screen_w + gx)];
        // Skip if another window/widget already painted text at the grip cell (occluded window).
        if (cell.ch != 0 && cell.ch != (uint32_t)' ' && cell.ch != (uint32_t)'+')
            continue;
        if (trace_cell(gx, gy))
            std::fprintf(stderr, "WRITE (%d,%d) ch='+' fn=resize_grip prev_ch=%u prev_bg=%u win=%s\n", gx, gy, cell.ch, cell.bg, w->Name);
        cell.ch = (uint32_t)'+';
        cell.fg = 245;
    }
}

// ── Fill triangle (scanline, ported from imtui drawTriangle) ─────────────────
// Top-bias Y boundary: ymin = floor(min_y), ydelta = floor(max_y) - ymin.
// Rect y=5..6 → ydelta=1 (row 5 only). AddLine quad y=M±0.5 → ydelta=1
// (row M-1 only), prevents 1-row leak past frame at PlotLines bottom edge.
// Tiny intra-row triangles (height < 1 cell) still get 1 row so they render.

void ImguiRendererNcurses::_draw_fill_triangle(ImVec2 p0, ImVec2 p1, ImVec2 p2, uint8_t bg_col, const ImVec4 & clip)
{
    const float min_yf = std::min({p0.y, p1.y, p2.y});
    const float max_yf = std::max({p0.y, p1.y, p2.y});
    int ymin = (int)min_yf;
    int ydelta = (int)max_yf - ymin;
    if (ydelta == 0 && max_yf > min_yf)
        ydelta = 1;
    if (ydelta <= 0)
        return;

    // ydelta=1: pick row containing triangle centroid, not floor(min_y).
    // Diagonal AddLine half-quads have top vertex barely below integer row
    // boundary; top-bias would fill the wrong row (above the line center).
    // For axis-aligned thin lines (plot edges, y=N±0.5), centroid stays at
    // line center row — same as top-bias modulo clip behavior.
    if (ydelta == 1)
    {
        const float cy = (p0.y + p1.y + p2.y) * (1.0f / 3.0f);
        const int cy_row = (int)cy;
        if (cy_row > ymin)
            ymin = cy_row;
    }

    if ((int)_scanline_range.size() < 2 * ydelta)
        _scanline_range.resize((size_t)(2 * ydelta));

    for (int y = 0; y < ydelta; y++)
    {
        _scanline_range[(size_t)(2 * y)] = 999999;
        _scanline_range[(size_t)(2 * y + 1)] = -999999;
    }

    scanline_extend((int)p0.x, (int)(p0.y - ymin), (int)p1.x, (int)(p1.y - ymin), ydelta, _scanline_range);
    scanline_extend((int)p1.x, (int)(p1.y - ymin), (int)p2.x, (int)(p2.y - ymin), ydelta, _scanline_range);
    scanline_extend((int)p2.x, (int)(p2.y - ymin), (int)p0.x, (int)(p0.y - ymin), ydelta, _scanline_range);

    const int clip_x0 = std::max(0, (int)clip.x);
    const int clip_x1 = std::min(_screen_w, (int)clip.z);
    const int clip_y0 = std::max(0, (int)clip.y);
    const int clip_y1 = std::min(_screen_h, (int)clip.w);

    for (int y = 0; y < ydelta; y++)
    {
        int x = _scanline_range[(size_t)(2 * y)];
        int len = 1 + _scanline_range[(size_t)(2 * y + 1)] - _scanline_range[(size_t)(2 * y)];
        if (len <= 0)
            continue;

        while (len-- > 0)
        {
            const int sy = y + ymin;
            if (x >= clip_x0 && x < clip_x1 && sy >= clip_y0 && sy < clip_y1)
            {
                TCell & cell = _screen[(size_t)(sy * _screen_w + x)];
                if (trace_cell(x, sy))
                    std::fprintf(stderr,
                                 "WRITE (%d,%d) ch=' ' fn=fill_tri prev_ch=%u prev_bg=%u "
                                 "p0=(%.3f,%.3f) p1=(%.3f,%.3f) p2=(%.3f,%.3f) bg=%u\n",
                                 x, sy, cell.ch, (unsigned)cell.bg, p0.x, p0.y, p1.x, p1.y, p2.x, p2.y,
                                 (unsigned)bg_col);
                cell.ch = (uint32_t)' ';
                cell.bg = bg_col;
            }
            ++x;
        }
    }
}

// ── classify_arrow() ─────────────────────────────────────────────────────────
// Returns '>', '<', '^', 'v' if p0/p1/p2 match stock imgui RenderArrow geometry.
// Two vertices share one coordinate (the base edge); the third is the apex.

uint32_t ImguiRendererNcurses::classify_arrow(ImVec2 p0, ImVec2 p1, ImVec2 p2)
{
    float xs[3] = {p0.x, p1.x, p2.x};
    float ys[3] = {p0.y, p1.y, p2.y};
    std::sort(xs, xs + 3);
    std::sort(ys, ys + 3);

    const float xg_lo = xs[1] - xs[0], xg_hi = xs[2] - xs[1];
    const float yg_lo = ys[1] - ys[0], yg_hi = ys[2] - ys[1];
    const float min_xg = std::min(xg_lo, xg_hi);
    const float min_yg = std::min(yg_lo, yg_hi);

    const float xspan = xs[2] - xs[0];
    const float yspan = ys[2] - ys[0];
    if (xspan < 0.01f || yspan < 0.01f)
        return 0;

    const float ratio = xspan / yspan;
    if (ratio < 0.4f || ratio > 2.5f)
        return 0;

    // RenderArrow base edge: two vertices aligned on one axis (gap < 0.05)
    const bool x_base = (min_xg < 0.05f);
    const bool y_base = (min_yg < 0.05f);
    if (!x_base && !y_base)
        return 0;
    if (x_base && y_base)
        return 0;

    if (x_base)
        return (uint32_t)((xg_hi > xg_lo) ? '>' : '<');
    return (uint32_t)((yg_hi > yg_lo) ? 'v' : '^');
}


// ── _draw_glyph_quad() ────────────────────────────────────────────────────────
// Renders one imgui text character. v0 is the top-left vertex of the quad
// (PrimRectUV vertex a) — contains the glyph position and atlas UV.

void ImguiRendererNcurses::_draw_glyph_quad(const ImDrawVert & v0, const ImDrawVert & v1,
                                            [[maybe_unused]] const ImDrawVert & v2, const ImVec4 & clip)
{
    if (_diag_enabled)
        _diag.glyph_quads++;

    // Use horizontal quad midpoint (TL.x + TR.x) / 2 to map glyph to its cell.
    // Variable-width glyphs (e.g. 'L') have a quad whose top-left is offset by
    // the font bearing — top-left round mis-mapped those into the next cell.
    // Row stays at the top edge (pen y) so descender glyphs (g, y, p, q, j)
    // whose quad extends one row below stay on the baseline row.
    const float mid_x = (v0.pos.x + v1.pos.x) * 0.5f;
    const int col = (int)std::floor(mid_x);
    const int row = (int)std::floor(v0.pos.y);

    // Float-tolerance clip: include glyph if any part of its 1×1 cell overlaps clip rect.
    // Integer _in_clip excluded chars at half-cell clip boundaries (e.g., child window edge).
    if (v0.pos.x + 1.0f <= clip.x || v0.pos.x >= clip.z || v0.pos.y + 1.0f <= clip.y || v0.pos.y >= clip.w || col < 0
        || col >= _screen_w || row < 0 || row >= _screen_h)
    {
        return;
    }

    const auto it = _uv_map.find(_pack_uv(v0.uv.x, v0.uv.y));
    if (std::getenv("SIHD_IMGUI_TRACE_GLYPH") && row == 79)
        std::fprintf(stderr, "GLYPH row=%d col=%d uv=(%.6f,%.6f) v0=(%.3f,%.3f) v1=(%.3f,%.3f) found=%d ch=%u\n",
                     row, col, v0.uv.x, v0.uv.y, v0.pos.x, v0.pos.y, v1.pos.x, v1.pos.y,
                     it != _uv_map.end() ? 1 : 0, it != _uv_map.end() ? it->second : 0);
    if (it != _uv_map.end())
    {
        TCell & cell = _screen[(size_t)(row * _screen_w + col)];
        if (trace_cell(col, row))
            std::fprintf(stderr, "WRITE (%d,%d) ch=glyph(%u) fn=glyph prev_ch=%u prev_bg=%u\n", col, row, it->second, cell.ch, cell.bg);
        cell.ch = it->second;
        cell.fg = col_to_ansi256(v0.col);
    }
    else
    {
        if (_diag_enabled)
            _diag.uv_misses++;
    }
}

// ── _draw_rect() ─────────────────────────────────────────────────────────────
// Handles a detected rect pair (2 triangles forming axis-aligned rectangle).
// Classifies based on FULL rect dimensions to avoid split-triangle artifacts.

void ImguiRendererNcurses::_draw_rect(float xmin, float ymin, float xmax, float ymax, ImU32 col, const ImVec4 & clip)
{
    if ((col >> 24) < kMinVisibleAlpha)
        return;

    xmin = std::max(xmin, clip.x);
    ymin = std::max(ymin, clip.y);
    xmax = std::min(xmax, clip.z);
    ymax = std::min(ymax, clip.w);
    if (xmin >= xmax || ymin >= ymax)
        return;

    const float width = xmax - xmin;
    const float height = ymax - ymin;

    // Modal/nav dim background: imgui fills the whole viewport with a translucent
    // overlay to dim windows behind a modal. The terminal has no alpha, so this
    // paints the entire screen solid gray and hides everything underneath. Skip it;
    // the modal still renders on top, which reads correctly without the dimming.
    if ((col >> 24) < 255 && width >= (float)_screen_w * 0.6f && height >= (float)_screen_h * 0.6f)
        return;

    // Slider grab handle: a SliderGrab-coloured rect. Render as a single 'I' at
    // the centre instead of a coverage block (matches thin grabs).
    // The inactive grab colour is unique; the active grab colour is shared with
    // ButtonHovered/CheckMark in the dark theme, so only collapse it when the rect
    // is handle-shaped (not a wide-flat button bar). Real grabs are ~w/h<=4; a
    // full-width button is ~w/h>=29.
    const bool is_grab_active = (col == _slider_grab_col_active);
    const bool is_slider_grab =
        (col == _slider_grab_col) || (is_grab_active && width <= height * 8.0f);
    if (is_slider_grab && height >= 1.0f)
    {
        const int cx = (int)std::floor((xmin + xmax) / 2.0f);
        const int cy = (int)std::floor((ymin + ymax) / 2.0f);
        if (_in_clip(cx, cy, clip))
        {
            TCell & cell = _screen[(size_t)(cy * _screen_w + cx)];
            if (trace_cell(cx, cy))
                std::fprintf(stderr, "WRITE (%d,%d) ch='I' fn=draw_rect_grab prev_ch=%u prev_bg=%u\n", cx, cy, cell.ch, cell.bg);
            cell.ch = (uint32_t)'I';
            cell.fg = col_to_ansi256(col);
        }
        return;
    }

    if (width < 0.3f && height >= 1.0f)
    {
        const int cx = (int)std::floor((xmin + xmax) / 2.0f);
        const int cy = (int)std::floor((ymin + ymax) / 2.0f);
        if (_in_clip(cx, cy, clip))
        {
            TCell & cell = _screen[(size_t)(cy * _screen_w + cx)];
            if (trace_cell(cx, cy))
                std::fprintf(stderr, "WRITE (%d,%d) ch='I' fn=draw_rect_I prev_ch=%u prev_bg=%u\n", cx, cy, cell.ch, cell.bg);
            cell.ch = (uint32_t)'I';
            cell.fg = col_to_ansi256(col);
        }
        return;
    }
    // Sub-cell vertical sliver — window border, no cell-level equivalent.
    // Histogram bars excepted: a tall 1px bar must render as a 1-cell column,
    // otherwise high sample counts (bar width < 1px) drop every tall bar.
    if (width < 1.0f && height > 1.0f && col != _plot_histogram_col)
        return;

    const uint8_t bg = col_to_ansi256_premul(col);

    // Snap edges that barely cross a cell boundary (sub-pixel float error) so a
    // ~1-tall/wide rect does not bleed into an extra row/column. eps tolerance.
    constexpr float kEdgeEps = 0.1f;
    int ix_min = (int)std::floor(xmin + kEdgeEps);
    int ix_max = (int)std::ceil(xmax - kEdgeEps);
    int iy_min = (int)std::floor(ymin + kEdgeEps);
    int iy_max = (int)std::ceil(ymax - kEdgeEps);
    if (ix_max <= ix_min)
        ix_max = ix_min + 1;
    if (iy_max <= iy_min)
        iy_max = iy_min + 1;

    const int clip_x0 = std::max(0, (int)clip.x);
    const int clip_x1 = std::min(_screen_w, (int)clip.z);
    const int clip_y0 = std::max(0, (int)clip.y);
    const int clip_y1 = std::min(_screen_h, (int)clip.w);

    // ImGuiCol_Separator default = (0.43,0.43,0.50,0.50) → ABGR 0x80806E6E.
    // With ItemSpacing.y=1, separator gets its own row — render as truly blank line.
    const bool is_separator = (col == 0x80806E6E) && (width >= 3.0f) && (height <= 1.0f);
    if (is_separator)
        return;

    for (int y = iy_min; y < iy_max; ++y)
    {
        if (y < clip_y0 || y >= clip_y1)
            continue;
        for (int x = ix_min; x < ix_max; ++x)
        {
            if (x < clip_x0 || x >= clip_x1)
                continue;
            TCell & cell = _screen[(size_t)(y * _screen_w + x)];
            if (trace_cell(x, y))
                std::fprintf(stderr, "WRITE (%d,%d) ch=' ' fn=draw_rect prev_ch=%u prev_bg=%u col=0x%08x bg=%u rect=(%.3f,%.3f,%.3f,%.3f)\n",
                             x, y, cell.ch, cell.bg, col, bg, xmin, ymin, xmax, ymax);
            cell.ch = (uint32_t)' ';
            cell.bg = bg;
        }
    }
}

// ── _draw_nonglyph_triangle() ─────────────────────────────────────────────────
// Handles standalone non-rect triangles (PathFillConvex fans: arrows, circles,
// resize grip). Most rects go through _draw_rect now.

void ImguiRendererNcurses::_draw_nonglyph_triangle(ImVec2 p0, ImVec2 p1, ImVec2 p2, ImU32 col, const ImVec4 & clip)
{
    if ((col >> 24) < kMinVisibleAlpha)
        return;

    if (clip.z - 1 <= clip.x || clip.w - 1 <= clip.y)
        return;

    p0.x = std::clamp(p0.x, clip.x, clip.z - 1);
    p0.y = std::clamp(p0.y, clip.y, clip.w - 1);
    p1.x = std::clamp(p1.x, clip.x, clip.z - 1);
    p1.y = std::clamp(p1.y, clip.y, clip.w - 1);
    p2.x = std::clamp(p2.x, clip.x, clip.z - 1);
    p2.y = std::clamp(p2.y, clip.y, clip.w - 1);

    const float xspan = std::max({p0.x, p1.x, p2.x}) - std::min({p0.x, p1.x, p2.x});
    const float yspan = std::max({p0.y, p1.y, p2.y}) - std::min({p0.y, p1.y, p2.y});

    if (xspan <= 1.2f && yspan <= 1.2f && xspan >= 0.4f && yspan >= 0.4f)
    {
        const float cross2 = std::abs((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
        const float bbox_area = std::max(xspan, 0.001f) * std::max(yspan, 0.001f);
        if (cross2 / bbox_area >= 0.5f)
        {
            const uint32_t arrow_ch = classify_arrow(p0, p1, p2);
            if (arrow_ch)
            {
                if (_diag_enabled)
                    _diag.arrows++;
                const ImVec2 center = tri_centroid(p0, p1, p2);
                const int ax = (int)center.x;
                const int ay = (int)center.y;
                if (std::getenv("SIHD_IMGUI_TRACE_ARROW"))
                    std::fprintf(stderr, "ARROW ch=%c ax=%d ay=%d p0=(%.3f,%.3f) p1=(%.3f,%.3f) p2=(%.3f,%.3f) in_clip=%d\n",
                                 (char)arrow_ch, ax, ay, p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, _in_clip(ax, ay, clip) ? 1 : 0);
                if (_in_clip(ax, ay, clip))
                {
                    TCell & cell = _screen[(size_t)(ay * _screen_w + ax)];
                    if (trace_cell(ax, ay))
                        std::fprintf(stderr, "WRITE (%d,%d) ch=arrow(%c) fn=arrow prev_ch=%u prev_bg=%u\n", ax, ay, (char)arrow_ch, cell.ch, cell.bg);
                    cell.ch = arrow_ch;
                    cell.fg = col_to_ansi256(col);
                }
                return;
            }
        }
    }

    _draw_fill_triangle(p0, p1, p2, col_to_ansi256_premul(col), clip);
}

// ── is_cross_pattern() ───────────────────────────────────────────────────────
// Given 12 vertices (4 triangles × 3 verts), return true if they form two
// crossing line segments (an X shape).  False for sequential line segments
// (PlotLines), parallel lines, or patterns that don't span enough area.

bool ImguiRendererNcurses::is_cross_pattern(const ImVec2 pts[12])
{
    float mnx = pts[0].x, mxx = pts[0].x;
    float mny = pts[0].y, mxy = pts[0].y;
    for (int vi = 1; vi < 12; ++vi)
    {
        mnx = std::min(mnx, pts[vi].x);
        mxx = std::max(mxx, pts[vi].x);
        mny = std::min(mny, pts[vi].y);
        mxy = std::max(mxy, pts[vi].y);
    }
    const float bw = mxx - mnx, bh = mxy - mny;

    if (bw < kSubCellThreshold || bh < kSubCellThreshold || bw > kCrossMaxSpan || bh > kCrossMaxSpan)
        return false;

    // Aspect ratio: a real X is roughly square
    const float ratio = bw / bh;
    if (ratio < 0.35f || ratio > 2.85f)
        return false;

    // Distinct position count: need spread on both axes
    float ux[12], uy[12];
    for (int vi = 0; vi < 12; ++vi)
    {
        ux[vi] = pts[vi].x;
        uy[vi] = pts[vi].y;
    }
    std::sort(ux, ux + 12);
    std::sort(uy, uy + 12);
    int nx = 1, ny = 1;
    for (int vi = 1; vi < 12; ++vi)
    {
        if (ux[vi] - ux[vi - 1] > kVertexEpsilon)
            ++nx;
        if (uy[vi] - uy[vi - 1] > kVertexEpsilon)
            ++ny;
    }
    if (nx <= 2 || ny <= 2)
        return false;

    // Centroid overlap: the two triangle pairs must have centroids close together.
    // A real X has both diagonals passing through the center.
    // Sequential line segments have offset centroids.
    float cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0;
    for (int vi = 0; vi < 6; ++vi)
    {
        cx0 += pts[vi].x;
        cy0 += pts[vi].y;
    }
    for (int vi = 6; vi < 12; ++vi)
    {
        cx1 += pts[vi].x;
        cy1 += pts[vi].y;
    }
    cx0 /= 6.0f; cy0 /= 6.0f;
    cx1 /= 6.0f; cy1 /= 6.0f;
    const float cdist = std::max(std::abs(cx0 - cx1), std::abs(cy0 - cy1));
    if (cdist > std::max(bw, bh) * 0.4f)
        return false;

    return true;
}

// ── _try_detect_cross() ──────────────────────────────────────────────────────
// Two AddLine calls (close button X) produce 4 consecutive non-glyph
// same-color triangles forming a diagonal cross. Returns 9 (extra elements
// consumed) on success, 0 otherwise.

// ── _try_detect_check() ──────────────────────────────────────────────────────
// imgui RenderCheckMark emits a 2-segment thick polyline in ImGuiCol_CheckMark
// colour — 4 triangles at our font scale. Fed to the fill path they smear a
// multi-cell blob over the 1-cell frame. Cluster the run of consecutive
// same-colour non-glyph triangles, write a single 'x' at the centroid cell
// (preserving the frame bg), and consume the whole run. Returns extra element
// count consumed, or 0 if the first triangle is not a checkmark.

unsigned int ImguiRendererNcurses::_try_detect_check(const ImDrawList *dl,
                                                     const ImDrawCmd & cmd,
                                                     unsigned int i,
                                                     const ImDrawVert & v0,
                                                     const ImVec4 & clip)
{
    if (_check_col == 0 || v0.col != _check_col)
        return 0;

    float mnx = v0.pos.x, mxx = v0.pos.x, mny = v0.pos.y, mxy = v0.pos.y;
    float sx = 0.0f, sy = 0.0f;
    unsigned int count = 0;
    for (unsigned int t = i; t + 2 < cmd.ElemCount; t += 3)
    {
        const ImDrawVert & a = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + t + 0]];
        const ImDrawVert & b = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + t + 1]];
        const ImDrawVert & c = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + t + 2]];
        if (a.col != _check_col || is_glyph_triangle(a, b, c))
            break;
        const ImVec2 tri[3] = {a.pos, b.pos, c.pos};
        for (int k = 0; k < 3; ++k)
        {
            mnx = std::min(mnx, tri[k].x);
            mxx = std::max(mxx, tri[k].x);
            mny = std::min(mny, tri[k].y);
            mxy = std::max(mxy, tri[k].y);
            sx += tri[k].x;
            sy += tri[k].y;
        }
        ++count;
    }

    if (count < 2)
        return 0;

    const float w = mxx - mnx;
    const float h = mxy - mny;
    if (w < 0.8f || w > 2.5f || h < 0.8f || h > 2.5f)
        return 0;

    const int cx = (int)std::floor(sx / (float)(count * 3));
    const int cy = (int)std::floor(sy / (float)(count * 3));
    if (_in_clip(cx, cy, clip))
    {
        TCell & cell = _screen[(size_t)(cy * _screen_w + cx)];
        if (trace_cell(cx, cy))
            std::fprintf(stderr, "WRITE (%d,%d) ch='x' fn=check prev_ch=%u prev_bg=%u\n", cx, cy, cell.ch, cell.bg);
        cell.ch = (uint32_t)'x';
        cell.fg = col_to_ansi256(_check_col);
    }
    return (count - 1) * 3;
}

unsigned int ImguiRendererNcurses::_try_detect_cross(const ImDrawList *dl,
                                                     const ImDrawCmd & cmd,
                                                     unsigned int i,
                                                     const ImDrawVert & v0,
                                                     const ImVec4 & clip)
{
    if (i + 9 >= cmd.ElemCount)
        return 0;

    ImVec2 pts[12];
    bool cross_ok = true;
    const ImU32 base_col = v0.col;

    pts[0] = v0.pos;
    pts[1] = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + 1]].pos;
    pts[2] = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + 2]].pos;

    for (unsigned int t = 1; t < 4 && cross_ok; ++t)
    {
        const unsigned int bi = i + t * 3;
        const ImDrawVert & a = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + bi + 0]];
        const ImDrawVert & b = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + bi + 1]];
        const ImDrawVert & c = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + bi + 2]];
        cross_ok = !is_glyph_triangle(a, b, c) && a.col == base_col;
        pts[t * 3 + 0] = a.pos;
        pts[t * 3 + 1] = b.pos;
        pts[t * 3 + 2] = c.pos;
    }

    if (!cross_ok)
        return 0;

    // Real X = 2 AddLine quads crossing at center → quad centroids coincide.
    // Polyline = connected segments → quad centroids offset by ~segment length.
    // Also reject if any vertex position shared (AA polyline emits dup verts
    // with different indices but matching positions at segment joints).
    {
        ImVec2 c0 {0, 0}, c1 {0, 0};
        for (int k = 0; k < 6; ++k)
        {
            c0.x += pts[k].x; c0.y += pts[k].y;
            c1.x += pts[k + 6].x; c1.y += pts[k + 6].y;
        }
        c0.x /= 6.0f; c0.y /= 6.0f;
        c1.x /= 6.0f; c1.y /= 6.0f;
        const float cdx = c0.x - c1.x, cdy = c0.y - c1.y;
        if (cdx * cdx + cdy * cdy > 0.25f)
            return 0;
        for (int a = 0; a < 6; ++a)
            for (int b = 6; b < 12; ++b)
            {
                const float dx = pts[a].x - pts[b].x;
                const float dy = pts[a].y - pts[b].y;
                if (dx * dx + dy * dy < kVertexEpsilon * kVertexEpsilon)
                    return 0;
            }
    }

    if (!is_cross_pattern(pts))
        return 0;

    float mnx = pts[0].x, mxx = pts[0].x;
    float mny = pts[0].y, mxy = pts[0].y;
    for (int vi = 1; vi < 12; ++vi)
    {
        mnx = std::min(mnx, pts[vi].x);
        mxx = std::max(mxx, pts[vi].x);
        mny = std::min(mny, pts[vi].y);
        mxy = std::max(mxy, pts[vi].y);
    }

    const int cx = (int)((mnx + mxx) / 2.0f);
    const int cy = (int)((mny + mxy) / 2.0f);
    if (_in_clip(cx, cy, clip))
    {
        TCell & cell = _screen[(size_t)(cy * _screen_w + cx)];
        if (trace_cell(cx, cy))
            std::fprintf(stderr, "WRITE (%d,%d) ch='X' fn=cross prev_ch=%u prev_bg=%u\n", cx, cy, cell.ch, cell.bg);
        cell.ch = (uint32_t)'X';
        cell.fg = col_to_ansi256(base_col);
    }
    return 9;
}

// ── _try_detect_rect_pair() ──────────────────────────────────────────────────
// Peek at next triangle: if same color, non-glyph, and combined 6 vertices
// form an axis-aligned rect, handle as single rect. Returns 3 on success, 0
// otherwise.

unsigned int ImguiRendererNcurses::_try_detect_rect_pair(const ImDrawList *dl,
                                                         const ImDrawCmd & cmd,
                                                         unsigned int i,
                                                         const ImDrawVert & v0,
                                                         const ImDrawVert & v1,
                                                         const ImDrawVert & v2,
                                                         const ImVec4 & clip)
{
    if (i + 3 >= cmd.ElemCount)
        return 0;

    const unsigned int ni = i + 3;
    const ImDrawVert & n0 = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + ni + 0]];
    const ImDrawVert & n1 = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + ni + 1]];
    const ImDrawVert & n2 = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + ni + 2]];

    if (is_glyph_triangle(n0, n1, n2) || n0.col != v0.col)
        return 0;

    const float rxmin = std::min({v0.pos.x, v1.pos.x, v2.pos.x, n0.pos.x, n1.pos.x, n2.pos.x});
    const float rxmax = std::max({v0.pos.x, v1.pos.x, v2.pos.x, n0.pos.x, n1.pos.x, n2.pos.x});
    const float rymin = std::min({v0.pos.y, v1.pos.y, v2.pos.y, n0.pos.y, n1.pos.y, n2.pos.y});
    const float rymax = std::max({v0.pos.y, v1.pos.y, v2.pos.y, n0.pos.y, n1.pos.y, n2.pos.y});

    const ImVec2 all[6] = {v0.pos, v1.pos, v2.pos, n0.pos, n1.pos, n2.pos};
    for (int vi = 0; vi < 6; ++vi)
    {
        const bool x_ok = (std::abs(all[vi].x - rxmin) < kVertexEpsilon)
                          || (std::abs(all[vi].x - rxmax) < kVertexEpsilon);
        const bool y_ok = (std::abs(all[vi].y - rymin) < kVertexEpsilon)
                          || (std::abs(all[vi].y - rymax) < kVertexEpsilon);
        if (!x_ok || !y_ok)
            return 0;
    }

    if (rxmax - rxmin < 0.5f && rymax - rymin < 0.5f)
        return 0;

    _draw_rect(rxmin, rymin, rxmax, rymax, v0.col, clip);
    return 3;
}

// ── _try_detect_line_quad() ──────────────────────────────────────────────────
// AddLine emits 2 tris with idx [0,1,2] and [0,2,3] sharing 2 verts. The 4 verts
// form a thin quad. Without this, sloped slivers in PlotLines hit arrow gate and
// render as '>' '<' '^' 'v'. Detection: index pair (i0==j0 && i2==j1). Fills
// both tris and returns 3 to skip the second.

unsigned int ImguiRendererNcurses::_try_detect_line_quad(const ImDrawList *dl,
                                                         const ImDrawCmd & cmd,
                                                         unsigned int i,
                                                         const ImDrawVert & v0,
                                                         const ImDrawVert & v1,
                                                         const ImDrawVert & v2,
                                                         const ImVec4 & clip)
{
    if (i + 3 >= cmd.ElemCount)
        return 0;
    const ImDrawIdx i0 = dl->IdxBuffer[cmd.IdxOffset + i + 0];
    const ImDrawIdx i2 = dl->IdxBuffer[cmd.IdxOffset + i + 2];
    const ImDrawIdx j0 = dl->IdxBuffer[cmd.IdxOffset + i + 3];
    const ImDrawIdx j1 = dl->IdxBuffer[cmd.IdxOffset + i + 4];
    if (i0 != j0 || i2 != j1)
        return 0;

    const ImDrawVert & n0 = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + 3]];
    const ImDrawVert & n1 = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + 4]];
    const ImDrawVert & n2 = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + 5]];
    if (n2.col != v0.col)
        return 0;
    if (is_glyph_triangle(n0, n1, n2) || is_glyph_triangle(v0, v1, v2))
        return 0;

    const uint8_t bg = col_to_ansi256_premul(v0.col);
    _draw_fill_triangle(v0.pos, v1.pos, v2.pos, bg, clip);
    _draw_fill_triangle(v0.pos, v2.pos, n2.pos, bg, clip);
    return 3;
}

// ── _rasterize_cmd() ─────────────────────────────────────────────────────────
// Process one ImDrawCmd: handle optional user callback, then walk its triangle
// list and dispatch each element to the glyph or geometry renderer.

void ImguiRendererNcurses::_rasterize_cmd(const ImDrawList *dl,
                                          const ImDrawCmd & cmd,
                                          int fb_w,
                                          int fb_h,
                                          const ImVec2 & clip_off,
                                          const ImVec2 & clip_scale)
{
    if (cmd.UserCallback)
    {
        if (cmd.UserCallback != ImDrawCallback_ResetRenderState)
            cmd.UserCallback(dl, &cmd);
        return;
    }

    const ImVec4 clip = compute_clip_rect(cmd, clip_off, clip_scale);
    if (clip.x >= fb_w || clip.y >= fb_h || clip.z < 0.0f || clip.w < 0.0f)
        return;

    for (unsigned int i = 0; i < cmd.ElemCount; i += 3)
    {
        const ImDrawVert & v0 = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + 0]];
        const ImDrawVert & v1 = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + 1]];
        const ImDrawVert & v2 = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + 2]];

        if (dump_active())
            dump_triangle(dump_state().cl, dump_state().cmd, i, clip, v0, v1, v2);

        if (is_glyph_triangle(v0, v1, v2))
        {
            _draw_glyph_quad(v0, v1, v2, clip);
            i += 3;
            continue;
        }

        if ((v0.col >> 24) < kMinVisibleAlpha)
            continue;

        const unsigned int check_consumed = _try_detect_check(dl, cmd, i, v0, clip);
        if (check_consumed)
        {
            i += check_consumed;
            continue;
        }

        const unsigned int cross_consumed = _try_detect_cross(dl, cmd, i, v0, clip);
        if (cross_consumed)
        {
            i += cross_consumed;
            continue;
        }

        const unsigned int rect_consumed = _try_detect_rect_pair(dl, cmd, i, v0, v1, v2, clip);
        if (rect_consumed)
        {
            i += rect_consumed;
            continue;
        }

        const unsigned int line_consumed = _try_detect_line_quad(dl, cmd, i, v0, v1, v2, clip);
        if (line_consumed)
        {
            i += line_consumed;
            continue;
        }

        _draw_nonglyph_triangle(v0.pos, v1.pos, v2.pos, v0.col, clip);
    }
}

// ── _rasterize_draw_list() ───────────────────────────────────────────────────
// Process all draw commands for one ImDrawList.

void ImguiRendererNcurses::_rasterize_draw_list(const ImDrawList *dl,
                                                int fb_w,
                                                int fb_h,
                                                const ImVec2 & clip_off,
                                                const ImVec2 & clip_scale)
{
    static int s_cl_idx = 0;
    if (dump_active())
        dump_cl_begin(s_cl_idx++);
    for (int ci = 0; ci < dl->CmdBuffer.Size; ++ci)
    {
        if (dump_active())
            dump_cmd_begin(ci);
        _rasterize_cmd(dl, dl->CmdBuffer[ci], fb_w, fb_h, clip_off, clip_scale);
    }
}

// ── _rasterize() ─────────────────────────────────────────────────────────────
// Top-level entry point: clear the cell buffer, then dispatch every draw list.

void ImguiRendererNcurses::_rasterize(ImDrawData *draw_data)
{
    const int fb_w = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    const int fb_h = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_w <= 0 || fb_h <= 0)
        return;

    if (_diag_enabled)
        _diag.frames++;

    std::fill(_screen.begin(), _screen.end(), TCell {0, 0, 0});

    dump_frame_begin();

    for (int n = 0; n < draw_data->CmdListsCount; ++n)
        _rasterize_draw_list(draw_data->CmdLists[n], fb_w, fb_h, draw_data->DisplayPos, draw_data->FramebufferScale);
}

// ── _get_or_alloc_color_pair() ────────────────────────────────────────────────
// Returns the ncurses color pair id for (fg, bg), allocating a new one if
// needed. Evicts the entire cache when the 256-pair limit is reached and forces
// a full redraw so no stale colors remain on screen.

short ImguiRendererNcurses::_get_or_alloc_color_pair(uint8_t fg, uint8_t bg)
{
    const uint32_t key = (uint32_t)bg * 256 + fg;
    const short cached = _pair_cache[key];
    if (cached >= 0)
        return cached;

    if (_next_pair_id >= COLOR_PAIRS - 1)
    {
        _next_pair_id = 1;
        std::fill(_pair_cache.begin(), _pair_cache.end(), (short)-1);
        std::fill(_screen_prev.begin(), _screen_prev.end(), TCell {(uint32_t)-1, 0xFFu, 0xFFu});
    }

    const short pid = _next_pair_id++;
    init_pair(pid, (short)fg, (short)bg);
    _pair_cache[key] = pid;
    return pid;
}

// ── _flush_row() ──────────────────────────────────────────────────────────────
// Writes one screen row to stdscr, batching consecutive cells with the same
// color pair to minimize attron() calls. Copies the row into _screen_prev.

void ImguiRendererNcurses::_flush_row(int y)
{
    move(y, 0);

    int ic = 0;
    int last_pair = -1;

    for (int x = 0; x < _screen_w; ++x)
    {
        // Skip bottom-right cell: writing it triggers a terminal scroll.
        if (y == _screen_h - 1 && x == _screen_w - 1)
            break;

        const TCell & cell = _screen[(size_t)(y * _screen_w + x)];
        const int pair_key = (int)cell.bg * 256 + (int)cell.fg;

        if (pair_key != last_pair)
        {
            if (ic > 0)
            {
                _row_buf[(size_t)ic] = '\0';
                addstr(_row_buf.data());
                ic = 0;
            }
            attron(COLOR_PAIR(_get_or_alloc_color_pair(cell.fg, cell.bg)));
            last_pair = pair_key;
        }

        _row_buf[(size_t)ic++] = (cell.ch >= 0x20 && cell.ch <= 0x7E) ? (char)cell.ch : ' ';
    }

    if (ic > 0)
    {
        _row_buf[(size_t)ic] = '\0';
        addstr(_row_buf.data());
    }

    std::memcpy(&_screen_prev[(size_t)(y * _screen_w)],
                &_screen[(size_t)(y * _screen_w)],
                (size_t)_screen_w * sizeof(TCell));
}

// ── _flush_to_ncurses() ───────────────────────────────────────────────────────
// Differential screen update: skips rows that did not change, then delegates
// each changed row to _flush_row(). Matches imtui DrawScreen() behavior.

void ImguiRendererNcurses::_flush_to_ncurses()
{
    if (_needs_full_redraw)
    {
        werase(stdscr);
        std::fill(_screen_prev.begin(), _screen_prev.end(), TCell {(uint32_t)-1, 0xFFu, 0xFFu});
        _needs_full_redraw = false;
    }

    if ((int)_row_buf.size() < _screen_w + 1)
        _row_buf.resize((size_t)(_screen_w + 1));

    for (int y = 0; y < _screen_h; ++y)
    {
        bool changed = false;
        for (int x = 0; x < _screen_w && !changed; ++x)
        {
            const size_t i = (size_t)(y * _screen_w + x);
            const TCell & c = _screen[i];
            const TCell & p = _screen_prev[i];
            changed = (c.ch != p.ch || c.fg != p.fg || c.bg != p.bg);
        }
        if (changed)
            _flush_row(y);
    }
}

// ── shutdown ──────────────────────────────────────────────────────────────────

void ImguiRendererNcurses::shutdown()
{
    if (_diag_enabled)
    {
        SIHD_LOG(
            info,
            "ImguiRendererNcurses diag — frames:{} glyphs:{} uv_miss:{} arrows:{}",
            _diag.frames,
            _diag.glyph_quads,
            _diag.uv_misses,
            _diag.arrows);
    }

    _is_init = false;
    _diag_enabled = false;
    _diag = {};
    _uv_map.clear();
    std::fill(_pair_cache.begin(), _pair_cache.end(), (short)-1);
    _screen.clear();
    _screen_prev.clear();
}

// ── Headless test API ───────────────────────────────────────────────────────

bool ImguiRendererNcurses::init_headless(int w, int h)
{
    _screen_w = w;
    _screen_h = h;
    const size_t n = (size_t)(w * h);
    _screen.assign(n, TCell {0, 0, 0});
    _screen_prev.assign(n, TCell {(uint32_t)-1, 0xFFu, 0xFFu});
    _is_init = true;
    return true;
}

const TCell & ImguiRendererNcurses::get_cell(int x, int y) const
{
    return _screen[(size_t)(y * _screen_w + x)];
}

void ImguiRendererNcurses::clear_screen()
{
    std::fill(_screen.begin(), _screen.end(), TCell {0, 0, 0});
}

void ImguiRendererNcurses::rasterize_headless(ImDrawData *draw_data)
{
    _rasterize(draw_data);
}

void ImguiRendererNcurses::draw_nonglyph_triangle(ImVec2 p0, ImVec2 p1, ImVec2 p2, ImU32 col, const ImVec4 & clip)
{
    _draw_nonglyph_triangle(p0, p1, p2, col, clip);
}

void ImguiRendererNcurses::draw_rect(float xmin, float ymin, float xmax, float ymax, ImU32 col, const ImVec4 & clip)
{
    _draw_rect(xmin, ymin, xmax, ymax, col, clip);
}

void ImguiRendererNcurses::draw_fill_triangle(ImVec2 p0, ImVec2 p1, ImVec2 p2, uint8_t bg, const ImVec4 & clip)
{
    _draw_fill_triangle(p0, p1, p2, bg, clip);
}

} // namespace sihd::imgui
