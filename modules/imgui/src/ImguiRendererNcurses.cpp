// wide-char api: glibc g++ gets it from _GNU_SOURCE, musl needs it asked for
#define NCURSES_WIDECHAR 1
#include <ncursesw/ncurses.h>
// ncurses may define `bool` as a macro (musl cross): undef it before std/imgui headers
#undef bool

#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cwchar>
#include <span>
#include <utility>
#include <vector>

#include <sihd/imgui/ImguiRendererNcurses.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::imgui
{

SIHD_LOGGER;

namespace
{

constexpr float vertex_epsilon = 0.01f;
constexpr float edge_epsilon = 1e-4f;
constexpr uint8_t min_visible_alpha = 8;
constexpr uint8_t opaque_alpha = 128;
// legacy stroke mode (kept for slider grabs and tab backgrounds) shifts every AddLine by half a unit
constexpr float legacy_line_offset = 0.5f;

constexpr uint8_t arm_up = 1;
constexpr uint8_t arm_down = 2;
constexpr uint8_t arm_left = 4;
constexpr uint8_t arm_right = 8;
constexpr uint8_t text_cell = 0x10;

constexpr uint32_t blank_char = ' ';
constexpr uint32_t braille_base = 0x2800;
constexpr uint32_t check_char = 0x2713;
constexpr uint32_t grip_char = 0x25E2;
constexpr uint32_t cross_char = 0x00D7;
constexpr uint32_t bullet_char = 0x2022;
constexpr uint32_t mixed_char = '-';

constexpr uint32_t arrow_right_char = 0x25B6;
constexpr uint32_t arrow_left_char = 0x25C0;
constexpr uint32_t arrow_down_char = 0x25BC;
constexpr uint32_t arrow_up_char = 0x25B2;

constexpr uint32_t image_char = 0x2592;
constexpr uint32_t upper_half_char = 0x2580;
constexpr uint32_t shade_char = 0x2591;
constexpr uint32_t full_block_char = 0x2588;

// arrows, checks, bullets and close crosses span about one cell: larger white shapes are user geometry
constexpr float mark_max_cells = 2.5f;

// role tag of the TextLink underline shades, past every ImGuiCol
constexpr int link_underline_tag = ImGuiCol_COUNT;
// TextLink brightens its colour when hovered and more when held, then draws its underline 0.2 darker
constexpr std::array<float, 3> link_brightening = {0.0f, 0.3f, 0.4f};
constexpr float link_underline_darkening = 0.2f;
constexpr float link_hover_hue_shift = 0.02f;

constexpr std::array<uint32_t, 16> box_chars = {
    blank_char,
    0x2502,
    0x2502,
    0x2502,
    0x2500,
    0x2518,
    0x2510,
    0x2524,
    0x2500,
    0x2514,
    0x250C,
    0x251C,
    0x2500,
    0x2534,
    0x252C,
    0x253C,
};
constexpr std::array<std::array<uint8_t, 4>, 2> braille_bits = {{{0x01, 0x02, 0x04, 0x40}, {0x08, 0x10, 0x20, 0x80}}};
constexpr std::array<uint32_t, 8> lower_blocks = {blank_char, 0x2581, 0x2582, 0x2583, 0x2584, 0x2585, 0x2586, 0x2587};
constexpr std::array<uint32_t, 8> left_blocks = {blank_char, 0x258F, 0x258E, 0x258D, 0x258C, 0x258B, 0x258A, 0x2589};

constexpr std::array<uint32_t, 16> ansi16 = {
    IM_COL32(0, 0, 0, 0),
    IM_COL32(205, 0, 0, 0),
    IM_COL32(0, 205, 0, 0),
    IM_COL32(205, 205, 0, 0),
    IM_COL32(0, 0, 238, 0),
    IM_COL32(205, 0, 205, 0),
    IM_COL32(0, 205, 205, 0),
    IM_COL32(229, 229, 229, 0),
    IM_COL32(127, 127, 127, 0),
    IM_COL32(255, 0, 0, 0),
    IM_COL32(0, 255, 0, 0),
    IM_COL32(255, 255, 0, 0),
    IM_COL32(92, 92, 255, 0),
    IM_COL32(255, 0, 255, 0),
    IM_COL32(0, 255, 255, 0),
    IM_COL32(255, 255, 255, 0),
};

uint32_t rgb_of(ImU32 col)
{
    return col & 0x00FFFFFFu;
}

int channel(uint32_t rgb, int shift)
{
    return (int)((rgb >> shift) & 0xFFu);
}

short nearest_base_color(uint32_t rgb, size_t count)
{
    size_t best = 0;
    int best_dist = -1;
    for (size_t i = 0; i < count; ++i)
    {
        const int dr = channel(rgb, IM_COL32_R_SHIFT) - channel(ansi16[i], IM_COL32_R_SHIFT);
        const int dg = channel(rgb, IM_COL32_G_SHIFT) - channel(ansi16[i], IM_COL32_G_SHIFT);
        const int db = channel(rgb, IM_COL32_B_SHIFT) - channel(ansi16[i], IM_COL32_B_SHIFT);
        const int dist = dr * dr + dg * dg + db * db;
        if (best_dist < 0 || dist < best_dist)
        {
            best_dist = dist;
            best = i;
        }
    }
    return (short)best;
}

// control characters are missing glyphs: imgui then makes a tab four blank cells wide
int glyph_index_from_codepoint(ImFontAtlas *, ImFontConfig *, ImWchar codepoint)
{
    const bool control = codepoint < 0x20 || (codepoint >= 0x7F && codepoint < 0xA0);
    return control ? 0 : (int)codepoint;
}

bool font_baked_init(ImFontAtlas *, ImFontConfig *, ImFontBaked *baked, void *)
{
    baked->Ascent = baked->Size;
    baked->Descent = 0.0f;
    return true;
}

// no bitmap: codepoint stored in the UVs (invalid PackId, imgui keeps them), advance = terminal column width
bool font_baked_load_glyph(ImFontAtlas *,
                           ImFontConfig *,
                           ImFontBaked *,
                           void *,
                           int glyph_index,
                           ImFontGlyph *out_glyph,
                           float *out_advance_x)
{
    int width = wcwidth((wchar_t)glyph_index);
    if (width < 0)
        width = 1;
    const float advance = (float)width;
    if (out_advance_x != nullptr)
    {
        *out_advance_x = advance;
        return true;
    }
    out_glyph->AdvanceX = advance;
    out_glyph->X0 = 0.0f;
    out_glyph->Y0 = 0.0f;
    out_glyph->X1 = advance;
    out_glyph->Y1 = 1.0f;
    out_glyph->U0 = (float)glyph_index;
    out_glyph->U1 = (float)glyph_index;
    out_glyph->V0 = 0.0f;
    out_glyph->V1 = 0.0f;
    out_glyph->Visible = width > 0 && glyph_index != (int)blank_char;
    return true;
}

// the font atlas keeps a pointer to its loader: it must outlive every ImGui context
const ImFontLoader & terminal_font_loader()
{
    static ImFontLoader loader;
    loader.Name = "terminal";
    loader.FontSrcGetGlyphIndexFromCodepoint = glyph_index_from_codepoint;
    loader.FontBakedInit = font_baked_init;
    loader.FontBakedLoadGlyph = font_baked_load_glyph;
    return loader;
}

const ImDrawVert & vertex_at(const ImDrawList *dl, const ImDrawCmd & cmd, unsigned int elem)
{
    return dl->VtxBuffer[(int)(cmd.VtxOffset + dl->IdxBuffer[(int)(cmd.IdxOffset + elem)])];
}

// imgui's PrimRect: four corners clockwise from the first, as triangles (0, 1, 2) and (0, 2, 3)
bool is_rect_quad(const ImDrawList *dl, const ImDrawCmd & cmd, unsigned int first)
{
    const std::span<const ImDrawIdx> indices(dl->IdxBuffer.Data + cmd.IdxOffset + first, 6);
    if (indices[3] != indices[0] || indices[4] != indices[2])
        return false;
    const ImVec2 corner = vertex_at(dl, cmd, first).pos;
    const ImVec2 next = vertex_at(dl, cmd, first + 1).pos;
    const ImVec2 opposite = vertex_at(dl, cmd, first + 2).pos;
    const ImVec2 last = vertex_at(dl, cmd, first + 5).pos;
    return next.x == opposite.x && next.y == corner.y && last.x == corner.x && last.y == opposite.y;
}

// the edge tests of point_in_triangle on two opposite rect sides, `across` long
bool between_rect_sides(float centre, float min, float max, float across)
{
    return (centre - min) * across >= -edge_epsilon && (max - centre) * across >= -edge_epsilon;
}

struct RectAxis
{
        float min;
        float max;
        float across;
        float clip_min;
        float clip_max;
        int screen;
};

bool covers_cell(const RectAxis & axis, int cell)
{
    const float centre = (float)cell + 0.5f;
    return between_rect_sides(centre, axis.min, axis.max, axis.across) && centre >= axis.clip_min
           && centre < axis.clip_max && cell >= 0 && cell < axis.screen;
}

// the covered cells of [first, end) form one interval
std::pair<int, int> covered_cells(const RectAxis & axis, int first, int end)
{
    while (first < end && !covers_cell(axis, first))
        ++first;
    while (end > first && !covers_cell(axis, end - 1))
        --end;
    return {first, end};
}

bool point_in_triangle(ImVec2 p, ImVec2 a, ImVec2 b, ImVec2 c)
{
    const float d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
    const float d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);
    const float d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);
    const bool has_neg = d1 < -edge_epsilon || d2 < -edge_epsilon || d3 < -edge_epsilon;
    const bool has_pos = d1 > edge_epsilon || d2 > edge_epsilon || d3 > edge_epsilon;
    return !(has_neg && has_pos);
}

int floor_cell(float v)
{
    return (int)std::floor(v);
}

// bounds a cell loop to the visible clip rect: cells are clipped again one by one, the slack keeps line ends off-screen
ImVec2 clamp_to_clip(ImVec2 point, const ImVec4 & clip)
{
    constexpr float slack = 2.0f;
    return {std::clamp(point.x, clip.x - slack, clip.z + slack), std::clamp(point.y, clip.y - slack, clip.w + slack)};
}

int round_cell(float v)
{
    return (int)std::floor(v + 0.5f);
}

uint8_t eighths_of(float coverage)
{
    return (uint8_t)std::clamp((int)std::lround(coverage * 8.0f), 0, 8);
}

ImU32 interpolate_color(ImVec2 point, const ImDrawVert & v0, const ImDrawVert & v1, const ImDrawVert & v2)
{
    const float area = (v1.pos.x - v0.pos.x) * (v2.pos.y - v0.pos.y) - (v1.pos.y - v0.pos.y) * (v2.pos.x - v0.pos.x);
    if (std::abs(area) < vertex_epsilon)
        return v0.col;
    const float weight_1 = ((point.x - v0.pos.x) * (v2.pos.y - v0.pos.y) - (point.y - v0.pos.y) * (v2.pos.x - v0.pos.x))
                           / area;
    const float weight_2 = ((v1.pos.x - v0.pos.x) * (point.y - v0.pos.y) - (v1.pos.y - v0.pos.y) * (point.x - v0.pos.x))
                           / area;
    const float weight_0 = 1.0f - weight_1 - weight_2;
    ImU32 out = 0;
    for (int shift = 0; shift <= IM_COL32_A_SHIFT; shift += 8)
    {
        const float mixed = weight_0 * (float)channel(v0.col, shift) + weight_1 * (float)channel(v1.col, shift)
                            + weight_2 * (float)channel(v2.col, shift);
        out |= (ImU32)std::clamp((int)std::lround(mixed), 0, 255) << shift;
    }
    return out;
}

// imgui spreads the UVs of an image linearly over its bounds (PrimRectUV, ShadeVertsLinearUV)
struct UvMap
{
        ImVec2 origin;
        ImVec2 per_unit;
};

UvMap image_uv_map(const ImDrawList *dl, const ImDrawCmd & cmd, unsigned int first, unsigned int end)
{
    const ImDrawVert *left = &vertex_at(dl, cmd, first);
    const ImDrawVert *right = left;
    const ImDrawVert *top = left;
    const ImDrawVert *bottom = left;
    for (unsigned int elem = first + 1; elem < end; ++elem)
    {
        const ImDrawVert & vertex = vertex_at(dl, cmd, elem);
        if (vertex.pos.x < left->pos.x)
            left = &vertex;
        if (vertex.pos.x > right->pos.x)
            right = &vertex;
        if (vertex.pos.y < top->pos.y)
            top = &vertex;
        if (vertex.pos.y > bottom->pos.y)
            bottom = &vertex;
    }
    const float width = right->pos.x - left->pos.x;
    const float height = bottom->pos.y - top->pos.y;
    return {.origin = ImVec2(left->uv.x, top->uv.y),
            .per_unit = ImVec2(width > 0.0f ? (right->uv.x - left->uv.x) / width : 0.0f,
                               height > 0.0f ? (bottom->uv.y - top->uv.y) / height : 0.0f)};
}

// the texels [first, end) a UV span covers, the nearest one when the span is narrower than a texel
std::pair<int, int> texel_span(float from, float to, int size)
{
    const float low = std::min(from, to) * (float)size;
    const float high = std::max(from, to) * (float)size;
    const int first = std::clamp((int)std::lround(low), 0, size);
    const int end = std::clamp((int)std::lround(high), 0, size);
    if (end > first)
        return {first, end};
    const int nearest = std::clamp(floor_cell((low + high) * 0.5f), 0, size - 1);
    return {nearest, nearest + 1};
}

// alpha-weighted mean of the texels under a UV rect: an Alpha8 texel is white coverage
ImU32 average_texels(const ImTextureData & texture, ImVec2 uv_from, ImVec2 uv_to)
{
    const std::span<const unsigned char> pixels(texture.Pixels, (size_t)texture.GetSizeInBytes());
    const auto [x0, x1] = texel_span(uv_from.x, uv_to.x, texture.Width);
    const auto [y0, y1] = texel_span(uv_from.y, uv_to.y, texture.Height);
    const bool alpha_only = texture.Format == ImTextureFormat_Alpha8;

    std::array<uint64_t, 3> weighted {};
    uint64_t alpha = 0;
    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            const size_t at = ((size_t)y * (size_t)texture.Width + (size_t)x) * (size_t)texture.BytesPerPixel;
            const uint64_t texel_alpha = alpha_only ? pixels[at] : pixels[at + 3];
            for (size_t c = 0; c < weighted.size(); ++c)
                weighted[c] += (alpha_only ? 255u : pixels[at + c]) * texel_alpha;
            alpha += texel_alpha;
        }
    }

    if (alpha == 0)
        return 0;
    const uint64_t count = (uint64_t)(x1 - x0) * (uint64_t)(y1 - y0);
    ImU32 out = (ImU32)((alpha + count / 2) / count) << IM_COL32_A_SHIFT;
    for (size_t c = 0; c < weighted.size(); ++c)
        out |= (ImU32)((weighted[c] + alpha / 2) / alpha) << (c * 8);
    return out;
}

ImU32 tinted(ImU32 texel, ImU32 tint)
{
    ImU32 out = 0;
    for (int shift = 0; shift <= IM_COL32_A_SHIFT; shift += 8)
        out |= (ImU32)((channel(texel, shift) * channel(tint, shift) + 127) / 255) << shift;
    return out;
}

uint32_t mix(uint32_t a, uint32_t b)
{
    uint32_t out = 0;
    for (int shift = 0; shift < IM_COL32_A_SHIFT; shift += 8)
        out |= (uint32_t)((channel(a, shift) + channel(b, shift) + 1) / 2) << shift;
    return out;
}

uint8_t span_arms(int at, int first, int last, uint8_t before, uint8_t after)
{
    if (first == last)
        return (uint8_t)(before | after);
    return (uint8_t)((at > first ? before : 0) | (at < last ? after : 0));
}

// a compact nav cursor hugs its item, an expanded one surrounds it: the ring alone cannot tell, imgui's nav rect can
ImRect nav_item_rect()
{
    const ImGuiContext & context = *ImGui::GetCurrentContext();
    if (context.NavWindow == nullptr)
        return {-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX};
    return ImGui::WindowRectRelToAbs(context.NavWindow, context.NavWindow->NavRectRel[context.NavLayer]);
}

std::pair<int, int> grab_span(float from, float to)
{
    if (to < from)
    {
        const int centre = floor_cell((from + to) * 0.5f);
        return {centre, centre + 1};
    }
    const int start = round_cell(from);
    return {start, std::max(start + 1, round_cell(to))};
}

} // namespace

ImguiRendererNcurses::ImguiRendererNcurses():
    _is_init(false),
    _unicode(true),
    _max_colors(256),
    _clear_color_ptr(nullptr),
    _clear_rgb(0),
    _white_uv(0.0f, 0.0f),
    _tagged_colors {},
    _screen_w(0),
    _screen_h(0)
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

uint32_t ImguiRendererNcurses::classify_arrow(ImVec2 p0, ImVec2 p1, ImVec2 p2)
{
    std::array<float, 3> xs = {p0.x, p1.x, p2.x};
    std::array<float, 3> ys = {p0.y, p1.y, p2.y};
    std::ranges::sort(xs);
    std::ranges::sort(ys);

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

ImVec4 ImguiRendererNcurses::compute_clip_rect(const ImDrawCmd & cmd, const ImVec2 & off, const ImVec2 & scale)
{
    return {(cmd.ClipRect.x - off.x) * scale.x,
            (cmd.ClipRect.y - off.y) * scale.y,
            (cmd.ClipRect.z - off.x) * scale.x,
            (cmd.ClipRect.w - off.y) * scale.y};
}

bool ImguiRendererNcurses::is_cross_pattern(const ImVec2 pts[cross_pattern_vertex_count])
{
    float mnx = pts[0].x, mxx = pts[0].x;
    float mny = pts[0].y, mxy = pts[0].y;
    for (size_t vi = 1; vi < cross_pattern_vertex_count; ++vi)
    {
        mnx = std::min(mnx, pts[vi].x);
        mxx = std::max(mxx, pts[vi].x);
        mny = std::min(mny, pts[vi].y);
        mxy = std::max(mxy, pts[vi].y);
    }
    const float bw = mxx - mnx, bh = mxy - mny;

    if (bw < 0.5f || bh < 0.5f || bw > 3.0f || bh > 3.0f)
        return false;

    const float ratio = bw / bh;
    if (ratio < 0.35f || ratio > 2.85f)
        return false;

    std::array<float, cross_pattern_vertex_count> ux;
    std::array<float, cross_pattern_vertex_count> uy;
    for (size_t vi = 0; vi < cross_pattern_vertex_count; ++vi)
    {
        ux[vi] = pts[vi].x;
        uy[vi] = pts[vi].y;
    }
    std::ranges::sort(ux);
    std::ranges::sort(uy);
    int nx = 1, ny = 1;
    for (size_t vi = 1; vi < cross_pattern_vertex_count; ++vi)
    {
        if (ux[vi] - ux[vi - 1] > vertex_epsilon)
            ++nx;
        if (uy[vi] - uy[vi - 1] > vertex_epsilon)
            ++ny;
    }
    if (nx <= 2 || ny <= 2)
        return false;

    float cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0;
    for (int vi = 0; vi < 6; ++vi)
    {
        cx0 += pts[vi].x;
        cy0 += pts[vi].y;
    }
    for (size_t vi = 6; vi < cross_pattern_vertex_count; ++vi)
    {
        cx1 += pts[vi].x;
        cy1 += pts[vi].y;
    }

    cx0 /= 6.0f;
    cy0 /= 6.0f;
    cx1 /= 6.0f;
    cy1 /= 6.0f;

    const float cdist = std::max(std::abs(cx0 - cx1), std::abs(cy0 - cy1));
    return cdist <= std::max(bw, bh) * 0.4f;
}

bool ImguiRendererNcurses::init()
{
    if (_is_init)
        return true;

    if (_clear_color_ptr == nullptr)
    {
        SIHD_LOG(error, "ImguiRendererNcurses: set_clear_color() must be called before init()");
        return false;
    }

    this->_setup_imgui();

    // single-byte locale: ncursesw would print non-ASCII as multi-cell escapes, breaking the grid
    _unicode = MB_CUR_MAX > 1;

    _max_colors = 0;
    if (has_colors() && start_color() == OK)
    {
        use_default_colors();
        _max_colors = COLORS;
    }

    this->_resize_grid(COLS, LINES);
    _is_init = true;
    return true;
}

void ImguiRendererNcurses::_resize_grid(int w, int h)
{
    _screen_w = w;
    _screen_h = h;
    const size_t cell_count = (size_t)(w * h);
    _screen.assign(cell_count, TCell {});
    _flushed.clear();
    _line_arms.assign(cell_count, 0);
    _braille.assign(cell_count, 0);
    _row_buf.resize((size_t)w);
}

size_t ImguiRendererNcurses::_index(int x, int y) const
{
    return (size_t)(y * _screen_w + x);
}

void ImguiRendererNcurses::_setup_imgui()
{
    ImGuiIO & io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;

    ImFontConfig cfg;
    cfg.FontLoader = &terminal_font_loader();
    cfg.SizePixels = 1.0f;
    io.Fonts->AddFont(&cfg);

    this->_apply_terminal_style();
}

void ImguiRendererNcurses::_apply_terminal_style()
{
    ImGuiIO & io = ImGui::GetIO();
    ImGuiStyle & style = ImGui::GetStyle();

    style.FontSizeBase = 1.0f;
    style.FontScaleMain = 1.0f;
    style.Alpha = 1.0f;
    // vertical padding keeps the first and last content rows clear of the window border
    style.WindowPadding = ImVec2(1.0f, 1.0f);
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowMinSize = ImVec2(4.0f, 2.0f);
    style.WindowTitleAlign = ImVec2(0.0f, 0.0f);
    style.WindowMenuButtonPosition = ImGuiDir_Left;

    style.ChildRounding = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 0.0f;
    style.PopupBorderSize = 1.0f;

    style.FramePadding = ImVec2(1.0f, 0.0f);
    style.FrameRounding = 0.0f;
    style.FrameBorderSize = 0.0f;

    style.ItemSpacing = ImVec2(1.0f, 0.0f);
    style.ItemInnerSpacing = ImVec2(1.0f, 0.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing = 2.0f;
    style.ColumnsMinSpacing = 1.0f;
    style.ScrollbarSize = 1.0f;
    style.ScrollbarRounding = 0.0f;
    style.ScrollbarPadding = 0.0f;
    style.GrabMinSize = 1.0f;
    style.GrabRounding = 0.0f;
    style.ImageBorderSize = 0.0f;

    style.TabRounding = 0.0f;
    style.TabBorderSize = 0.0f;
    style.TabBarBorderSize = 0.0f;
    style.TabBarOverlineSize = 1.0f;

    style.TreeLinesSize = 1.0f;
    style.DragDropTargetRounding = 0.0f;
    style.DragDropTargetBorderSize = 1.0f;
    style.DragDropTargetPadding = 0.0f;

    style.LogSliderDeadzone = 0.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.0f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
    // windows opening at imgui's default (60,60) pixels stay grabbable by their title bar on a small terminal
    style.DisplayWindowPadding = ImVec2(4.0f, 1.0f);
    style.DisplaySafeAreaPadding = ImVec2(0.0f, 0.0f);
    style.CellPadding = ImVec2(1.0f, 0.0f);

    // only scales the hardcoded tooltip offset from the mouse (24x10 pixels), the cursor itself is the terminal's
    style.MouseCursorScale = 0.1f;
    style.AntiAliasedLines = false;
    style.AntiAliasedFill = false;
    style.AntiAliasedLinesUseTex = false;
    style.CurveTessellationTol = 1.25f;
    style.CircleTessellationMaxError = 5.0f;

    io.MouseDragThreshold = 1.0f;
    io.ConfigWindowsResizeFromEdges = false;
    io.ConfigDebugDrawListDefaultsToStrokeLegacy = true;

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);

    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.70f, 0.70f, 0.70f, 0.60f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.90f, 0.90f, 0.90f, 0.80f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.95f);

    // resize border hover highlight is a 2-cell bar at terminal scale
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // must stay > 0 (imgui asserts); the 4.0 default steals clicks from the neighbouring cells
    style.WindowBorderHoverPadding = 0.5f;

    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextPadding = ImVec2(1.0f, 0.0f);
    style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);

    this->_tag_style_colors();
}

void ImguiRendererNcurses::_tag_style_colors()
{
    ImGuiStyle & style = ImGui::GetStyle();
    _rgb_roles.clear();
    for (int idx = 0; idx < ImGuiCol_COUNT; ++idx)
    {
        const ImU32 packed = ImGui::ColorConvertFloat4ToU32(style.Colors[idx]);
        const int red = channel(packed, IM_COL32_R_SHIFT);
        uint32_t rgb = rgb_of(packed);
        for (int step = 1; _rgb_roles.contains(rgb); ++step)
        {
            const int candidate = (step % 2 != 0) ? red + (step + 1) / 2 : red - step / 2;
            if (candidate >= 0 && candidate <= 255)
                rgb = (rgb_of(packed) & ~(0xFFu << IM_COL32_R_SHIFT)) | ((uint32_t)candidate << IM_COL32_R_SHIFT);
        }
        _rgb_roles.emplace(rgb, idx);
        style.Colors[idx] = ImGui::ColorConvertU32ToFloat4(rgb | (packed & IM_COL32_A_MASK));
        _tagged_colors[(size_t)idx] = style.Colors[idx];
    }
    this->_tag_link_underlines();
}

// TextLink derives its underline colour from ImGuiCol_TextLink in HSV: tag each shade it can take
void ImguiRendererNcurses::_tag_link_underlines()
{
    const ImVec4 link = ImGui::GetStyle().Colors[ImGuiCol_TextLink];
    float hue = 0.0f;
    float saturation = 0.0f;
    float value = 0.0f;
    ImGui::ColorConvertRGBtoHSV(link.x, link.y, link.z, hue, saturation, value);
    for (const float brightening : link_brightening)
    {
        const float shade_hue = brightening > 0.0f ? ImFmod(hue + link_hover_hue_shift, 1.0f) : hue;
        const float shade_value = ImSaturate(ImSaturate(value + brightening) - link_underline_darkening);
        ImVec4 shade = link;
        ImGui::ColorConvertHSVtoRGB(shade_hue, saturation, shade_value, shade.x, shade.y, shade.z);
        _rgb_roles.try_emplace(rgb_of(ImGui::ColorConvertFloat4ToU32(shade)), link_underline_tag);
    }
}

ImguiRendererNcurses::Role ImguiRendererNcurses::_role_of(ImU32 col) const
{
    const auto it = _rgb_roles.find(rgb_of(col));
    if (it == _rgb_roles.end())
        return Role::fill;
    switch (it->second)
    {
        case link_underline_tag:
            return Role::link_underline;
        case ImGuiCol_Separator:
            return Role::separator;
        case ImGuiCol_Border:
        case ImGuiCol_SeparatorHovered:
        case ImGuiCol_SeparatorActive:
        case ImGuiCol_TreeLines:
        case ImGuiCol_DragDropTarget:
        case ImGuiCol_NavWindowingHighlight:
            return Role::line;
        // table rows are one cell tall: a horizontal border would sit on the row text
        case ImGuiCol_TableBorderStrong:
        case ImGuiCol_TableBorderLight:
            return Role::table_line;
        case ImGuiCol_CheckMark:
            return Role::check;
        case ImGuiCol_ResizeGrip:
        case ImGuiCol_ResizeGripHovered:
        case ImGuiCol_ResizeGripActive:
            return Role::grip;
        case ImGuiCol_Text:
        case ImGuiCol_TextDisabled:
            return Role::text_mark;
        case ImGuiCol_PlotLines:
        case ImGuiCol_PlotLinesHovered:
            return Role::plot_line;
        case ImGuiCol_PlotHistogram:
        case ImGuiCol_PlotHistogramHovered:
            return Role::histogram;
        case ImGuiCol_NavCursor:
            return Role::nav;
        case ImGuiCol_TextSelectedBg:
            return Role::reverse;
        case ImGuiCol_ScrollbarBg:
            return Role::scrollbar;
        case ImGuiCol_ScrollbarGrab:
        case ImGuiCol_ScrollbarGrabHovered:
        case ImGuiCol_ScrollbarGrabActive:
            return Role::scrollbar_grab;
        case ImGuiCol_ModalWindowDimBg:
        case ImGuiCol_NavWindowingDimBg:
            return Role::dim;
        case ImGuiCol_TabSelectedOverline:
        case ImGuiCol_TabDimmedSelectedOverline:
            return Role::underline;
        case ImGuiCol_SliderGrab:
        case ImGuiCol_SliderGrabActive:
            return Role::grab;
        case ImGuiCol_Tab:
        case ImGuiCol_TabHovered:
        case ImGuiCol_TabSelected:
        case ImGuiCol_TabDimmed:
        case ImGuiCol_TabDimmedSelected:
            return Role::tab;
        case ImGuiCol_InputTextCursor:
        case ImGuiCol_BorderShadow:
            return Role::hidden;
        default:
            return Role::fill;
    }
}

void ImguiRendererNcurses::new_frame()
{
    if (ImGui::GetCurrentContext() == nullptr || _rgb_roles.empty())
        return;
    const ImGuiStyle & style = ImGui::GetStyle();
    for (int idx = 0; idx < ImGuiCol_COUNT; ++idx)
    {
        const ImVec4 & current = style.Colors[idx];
        const ImVec4 & tagged = _tagged_colors[(size_t)idx];
        if (current.x != tagged.x || current.y != tagged.y || current.z != tagged.z || current.w != tagged.w)
        {
            this->_tag_style_colors();
            return;
        }
    }
}

void ImguiRendererNcurses::resize()
{
    this->_resize_grid(COLS, LINES);
}

void ImguiRendererNcurses::render(ImDrawData *draw_data)
{
    if (_screen_w != COLS || _screen_h != LINES)
        this->resize();

    this->_rasterize(draw_data);
    this->_flush_to_ncurses();
}

void ImguiRendererNcurses::_ack_textures(ImDrawData *draw_data) const
{
    if (draw_data->Textures == nullptr)
        return;
    for (ImTextureData *tex : *draw_data->Textures)
    {
        if (tex->Status == ImTextureStatus_WantCreate)
        {
            tex->SetTexID((ImTextureID)1);
            tex->SetStatus(ImTextureStatus_OK);
        }
        else if (tex->Status == ImTextureStatus_WantUpdates)
        {
            tex->SetStatus(ImTextureStatus_OK);
        }
        else if (tex->Status == ImTextureStatus_WantDestroy)
        {
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }
    }
}

void ImguiRendererNcurses::_rasterize(ImDrawData *draw_data)
{
    this->_ack_textures(draw_data);

    const int fb_w = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    const int fb_h = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_w <= 0 || fb_h <= 0)
        return;

    _white_uv = ImGui::GetIO().Fonts->TexUvWhitePixel;
    _clear_rgb = _clear_color_ptr != nullptr ? rgb_of(ImGui::ColorConvertFloat4ToU32(*_clear_color_ptr)) : 0;
    std::ranges::fill(_screen, TCell {});
    std::ranges::fill(_line_arms, 0);
    std::ranges::fill(_braille, 0);

    for (const ImDrawList *dl : draw_data->CmdLists)
        this->_rasterize_list(dl, *draw_data, ImVec2((float)fb_w, (float)fb_h));
}

void ImguiRendererNcurses::_rasterize_list(const ImDrawList *dl, const ImDrawData & draw_data, const ImVec2 & fb_size)
{
    const ImDrawCallback reset_callback = ImGui::GetPlatformIO().DrawCallback_ResetRenderState;
    for (const ImDrawCmd & cmd : dl->CmdBuffer)
    {
        if (cmd.UserCallback != nullptr)
        {
            if (cmd.UserCallback != reset_callback)
                cmd.UserCallback(dl, &cmd);
            continue;
        }
        ImVec4 full_clip = compute_clip_rect(cmd, draw_data.DisplayPos, draw_data.FramebufferScale);
        // a menu bar without title bar above clips from round(top + border / 2): its own bottom at font size 1
        if (full_clip.w == full_clip.y)
            full_clip.y -= 1.0f;
        const ImVec4 clip(std::max(full_clip.x, 0.0f),
                          std::max(full_clip.y, 0.0f),
                          std::min(full_clip.z, fb_size.x),
                          std::min(full_clip.w, fb_size.y));
        if (clip.x >= clip.z || clip.y >= clip.w)
            continue;
        this->_rasterize_cmd(dl, cmd, clip);
    }
    this->_draw_clipped_table_headers(dl, fb_size);
    // a nav cursor is drawn before the frame of its item: apply it once the whole window is drawn
    for (const auto & [prim, clip] : _deferred_nav)
        this->_set_attribute(prim, Role::nav, clip);
    _deferred_nav.clear();
}

ImguiRendererNcurses::Primitive
    ImguiRendererNcurses::_collect_primitive(const ImDrawList *dl, const ImDrawCmd & cmd, unsigned int first) const
{
    Primitive prim;
    prim.first = first;
    prim.col = vertex_at(dl, cmd, first).col;
    prim.min = ImVec2(FLT_MAX, FLT_MAX);
    prim.max = ImVec2(-FLT_MAX, -FLT_MAX);
    const bool untextured = this->_is_untextured(vertex_at(dl, cmd, first));

    unsigned int min_index = UINT_MAX;
    unsigned int max_index = 0;
    unsigned int elem = first;
    for (; elem + 2 < cmd.ElemCount; elem += 3)
    {
        std::array<unsigned int, 3> indices {};
        for (unsigned int k = 0; k < 3; ++k)
            indices[k] = dl->IdxBuffer[(int)(cmd.IdxOffset + elem + k)];
        const ImDrawVert & lead_vertex = dl->VtxBuffer[(int)(cmd.VtxOffset + indices[0])];
        const unsigned int tri_min = std::ranges::min(indices);
        // a primitive owns a fresh vertex block: a triangle past it starts the next primitive
        const bool next_primitive = elem > first
                                    && (lead_vertex.col != prim.col || this->_is_untextured(lead_vertex) != untextured
                                        || tri_min > max_index);
        if (next_primitive)
            break;
        min_index = std::min(min_index, tri_min);
        max_index = std::max(max_index, std::ranges::max(indices));
        for (unsigned int index : indices)
        {
            const ImDrawVert & vertex = dl->VtxBuffer[(int)(cmd.VtxOffset + index)];
            prim.min = ImMin(prim.min, vertex.pos);
            prim.max = ImMax(prim.max, vertex.pos);
            prim.gradient |= vertex.col != prim.col;
        }
    }
    prim.end = elem;
    prim.first_vertex = min_index;
    prim.vertex_count = max_index - min_index + 1;
    prim.rect = !prim.gradient && prim.end - prim.first == 6 && is_rect_quad(dl, cmd, prim.first);
    return prim;
}

std::optional<ImU32> ImguiRendererNcurses::_sample_primitive(const ImDrawList *dl,
                                                             const ImDrawCmd & cmd,
                                                             const Primitive & prim,
                                                             int x,
                                                             int y) const
{
    const ImVec2 center((float)x + 0.5f, (float)y + 0.5f);
    if (prim.rect)
    {
        // the diagonal both triangles share needs no test
        const bool inside = between_rect_sides(center.x, prim.min.x, prim.max.x, prim.max.y - prim.min.y)
                            && between_rect_sides(center.y, prim.min.y, prim.max.y, prim.max.x - prim.min.x);
        return inside ? std::optional<ImU32>(prim.col) : std::nullopt;
    }
    for (unsigned int elem = prim.first; elem < prim.end; elem += 3)
    {
        const ImDrawVert & v0 = vertex_at(dl, cmd, elem);
        const ImDrawVert & v1 = vertex_at(dl, cmd, elem + 1);
        const ImDrawVert & v2 = vertex_at(dl, cmd, elem + 2);
        if (point_in_triangle(center, v0.pos, v1.pos, v2.pos))
            return prim.gradient ? interpolate_color(center, v0, v1, v2) : prim.col;
    }
    return std::nullopt;
}

// a scrolling table with outer horizontal borders clips one border unit, a whole row, off its top: its header is culled
void ImguiRendererNcurses::_draw_clipped_table_headers(const ImDrawList *dl, const ImVec2 & fb_size)
{
    ImGuiContext & context = *ImGui::GetCurrentContext();
    for (int index = 0; index < context.Tables.GetMapSize(); ++index)
    {
        const ImGuiTable *table = context.Tables.TryGetMapData(index);
        const bool culled_header = table != nullptr && table->LastFrameActive == context.FrameCount
                                   && table->InnerWindow != table->OuterWindow && table->InnerWindow->DrawList == dl
                                   && table->IsUsingHeaders && (table->Flags & ImGuiTableFlags_BordersOuterH) != 0;
        if (culled_header)
            this->_draw_table_header(*table, fb_size);
    }
}

void ImguiRendererNcurses::_draw_table_header(const ImGuiTable & table, const ImVec2 & fb_size)
{
    const float y = table.OuterRect.Min.y;
    const ImVec4 row_clip(std::max(table.BgClipRect.Min.x, 0.0f),
                          std::max(y, 0.0f),
                          std::min(table.BgClipRect.Max.x, fb_size.x),
                          std::min({y + 1.0f, table.InnerWindow->ClipRect.Max.y, fb_size.y}));
    // background only: the column borders crossing the row stay
    const ImU32 header_bg = ImGui::GetColorU32(ImGuiCol_TableHeaderBg);
    const int row = floor_cell(y);
    for (int x = floor_cell(row_clip.x); x < (int)std::ceil(row_clip.z); ++x)
    {
        if (!this->_in_clip(x, row, row_clip))
            continue;
        TCell & cell = this->_cell(x, row);
        cell.bg = this->_blend(this->_cell_bg(cell), header_bg);
        cell.has_bg = true;
    }

    const ImU32 text_col = ImGui::GetColorU32(ImGuiCol_Text);
    for (int order = 0; order < table.ColumnsCount; ++order)
    {
        const int column_n = table.DisplayOrderToIndex[order];
        const ImGuiTableColumn & column = table.Columns[column_n];
        if (!column.IsVisibleX || (column.Flags & ImGuiTableColumnFlags_NoHeaderLabel) != 0)
            continue;
        const ImVec4 cell_clip(std::max(row_clip.x, column.ClipRect.Min.x),
                               row_clip.y,
                               std::min(row_clip.z, column.ClipRect.Max.x),
                               row_clip.w);
        const char *name = ImGui::TableGetColumnName(&table, column_n);
        this->_draw_label(column.WorkMinX, y, name, ImGui::FindRenderedTextEnd(name), text_col, cell_clip);
    }
}

void ImguiRendererNcurses::_draw_label(float x,
                                       float y,
                                       const char *text,
                                       const char *text_end,
                                       ImU32 col,
                                       const ImVec4 & clip)
{
    ImDrawVert left {};
    left.pos = ImVec2(x, y);
    left.col = col;
    while (text < text_end)
    {
        unsigned int codepoint = 0;
        text += ImTextCharFromUtf8(&codepoint, text, text_end);
        const int width = wcwidth((wchar_t)codepoint);
        if (width <= 0)
            continue;
        ImDrawVert right = left;
        right.pos.x += (float)width;
        left.uv.x = (float)codepoint;
        if (codepoint != blank_char)
            this->_draw_glyph(left, right, clip);
        left.pos.x = right.pos.x;
    }
}

void ImguiRendererNcurses::_draw_image(const Primitive & prim, const ImVec4 & clip)
{
    const ImVec2 min = clamp_to_clip(prim.min, clip);
    const ImVec2 max = clamp_to_clip(prim.max, clip);
    for (int y = round_cell(min.y); y < round_cell(max.y); ++y)
    {
        for (int x = round_cell(min.x); x < round_cell(max.x); ++x)
            this->_draw_mark(x, y, this->_pick(image_char, '#'), prim.col, clip);
    }
}

void ImguiRendererNcurses::_draw_pixels(const ImDrawList *dl,
                                        const ImDrawCmd & cmd,
                                        const Primitive & prim,
                                        const ImTextureData & texture,
                                        const ImVec4 & clip)
{
    const UvMap uv = image_uv_map(dl, cmd, prim.first, prim.end);
    const ImVec2 min = clamp_to_clip(prim.min, clip);
    const ImVec2 max = clamp_to_clip(prim.max, clip);
    for (int y = round_cell(min.y); y < round_cell(max.y); ++y)
    {
        const float v_top = uv.origin.y + ((float)y - prim.min.y) * uv.per_unit.y;
        const float v_middle = v_top + uv.per_unit.y * 0.5f;
        const float v_bottom = v_top + uv.per_unit.y;
        for (int x = round_cell(min.x); x < round_cell(max.x); ++x)
        {
            if (!this->_in_clip(x, y, clip))
                continue;
            const float u_left = uv.origin.x + ((float)x - prim.min.x) * uv.per_unit.x;
            const float u_right = u_left + uv.per_unit.x;
            const ImU32 top = average_texels(texture, ImVec2(u_left, v_top), ImVec2(u_right, v_middle));
            const ImU32 bottom = average_texels(texture, ImVec2(u_left, v_middle), ImVec2(u_right, v_bottom));

            const size_t index = this->_index(x, y);
            const uint32_t under = this->_cell_bg(_screen[index]);
            const uint32_t top_rgb = this->_blend(under, tinted(top, prim.col));
            const uint32_t bottom_rgb = this->_blend(under, tinted(bottom, prim.col));
            if (_unicode)
                _screen[index] = TCell {.ch = upper_half_char, .fg = top_rgb, .bg = bottom_rgb, .has_bg = true};
            else
                _screen[index] = TCell {.ch = blank_char, .bg = mix(top_rgb, bottom_rgb), .has_bg = true};
            _line_arms[index] = 0;
            _braille[index] = 0;
        }
    }
}

void ImguiRendererNcurses::_rasterize_cmd(const ImDrawList *dl, const ImDrawCmd & cmd, const ImVec4 & clip)
{
    // the font atlas holds the glyph codepoints in its UVs: any other texture is an image, pixels or a raw texture id
    ImTextureData *texture = cmd.TexRef._TexData;
    const bool user_texture = texture == nullptr || !ImGui::GetIO().Fonts->TexList.contains(texture);
    const bool has_pixels = user_texture && texture != nullptr && texture->Pixels != nullptr && texture->Width > 0
                            && texture->Height > 0 && _max_colors > 0;
    unsigned int elem = 0;
    while (elem + 2 < cmd.ElemCount)
    {
        const ImDrawVert & v0 = vertex_at(dl, cmd, elem);
        if (!user_texture && v0.uv.x >= 1.0f)
        {
            this->_draw_glyph(v0, vertex_at(dl, cmd, elem + 1), clip);
            elem += 3;
            continue;
        }

        const Primitive prim = this->_collect_primitive(dl, cmd, elem);
        elem = prim.end;
        // textured but no glyph: an image, the font atlas shown as one included
        if (has_pixels)
        {
            this->_draw_pixels(dl, cmd, prim, *texture, clip);
            continue;
        }
        if (user_texture || !this->_is_untextured(v0))
        {
            this->_draw_image(prim, clip);
            continue;
        }
        if ((prim.col >> IM_COL32_A_SHIFT) < min_visible_alpha)
            continue;

        // imgui draws its style colours flat: a gradient is user geometry, whatever colour it starts with
        const Role role = prim.gradient ? Role::fill : this->_role_of(prim.col);
        switch (role)
        {
            case Role::fill:
                this->_draw_fill(dl, cmd, prim, clip);
                break;
            case Role::histogram:
                this->_draw_histogram(dl, cmd, prim, clip);
                break;
            case Role::line:
            case Role::table_line:
                this->_draw_line(dl, cmd, prim, role == Role::table_line, clip);
                break;
            case Role::separator:
                this->_draw_separator(dl, cmd, prim, clip);
                break;
            case Role::check:
                // a check mark is a stroked polyline, a mixed value a filled rect
                this->_draw_mark(floor_cell((prim.min.x + prim.max.x) * 0.5f),
                                 floor_cell((prim.min.y + prim.max.y) * 0.5f),
                                 prim.vertex_count == 4 ? mixed_char : this->_pick(check_char, 'x'),
                                 prim.col,
                                 clip);
                break;
            case Role::grip:
                this->_draw_mark((int)std::ceil(prim.max.x) - 1,
                                 (int)std::ceil(prim.max.y) - 1,
                                 this->_pick(grip_char, '+'),
                                 prim.col,
                                 clip);
                break;
            case Role::text_mark:
                elem = this->_draw_text_mark(dl, cmd, prim, clip);
                break;
            case Role::plot_line:
                this->_draw_plot_line(dl, cmd, prim, clip);
                break;
            case Role::grab:
                this->_draw_grab(dl, cmd, prim, clip);
                break;
            case Role::tab:
                this->_draw_tab(dl, cmd, prim, clip);
                break;
            case Role::scrollbar:
                this->_draw_shaded_fill(dl,
                                        cmd,
                                        prim,
                                        this->_pick(shade_char, blank_char),
                                        ImGui::GetColorU32(ImGuiCol_ScrollbarGrab),
                                        clip);
                break;
            case Role::scrollbar_grab:
                this->_draw_shaded_fill(dl, cmd, prim, this->_pick(full_block_char, '#'), prim.col, clip);
                break;
            case Role::nav:
                // the nav cursor is a 2-unit ring, the box-select rectangle a 1-unit one
                if (this->_ring_thickness(dl, cmd, prim) < 1.5f)
                    this->_draw_line(dl, cmd, prim, false, clip);
                else
                    _deferred_nav.emplace_back(prim, clip);
                break;
            case Role::dim:
            case Role::underline:
            case Role::link_underline:
            case Role::reverse:
                this->_set_attribute(prim, role, clip);
                break;
            case Role::hidden:
                break;
        }
    }
    this->_join_table_borders(clip);
}

void ImguiRendererNcurses::_draw_glyph(const ImDrawVert & top_left, const ImDrawVert & top_right, const ImVec4 & clip)
{
    // imgui keeps a glyph starting on the clip edge, cut down to zero width
    const int x = round_cell(top_left.pos.x);
    const int y = round_cell(top_left.pos.y);
    if (top_right.pos.x - top_left.pos.x < 0.5f || !this->_in_clip(x, y, clip))
        return;

    const int width = std::max(1, round_cell(top_right.pos.x) - x);
    TCell & cell = this->_cell(x, y);
    const size_t index = this->_index(x, y);
    cell.ch = (uint32_t)top_left.uv.x;
    cell.fg = this->_blend(this->_cell_bg(cell), top_left.col);
    cell.wide_tail = false;
    _line_arms[index] = text_cell;
    _braille[index] = 0;

    // imgui emits no geometry for spaces: a one-cell border gap between two glyphs (title words) is one
    if (x >= 2)
    {
        const uint8_t gap_arms = _line_arms[index - 1];
        const bool horizontal_gap = gap_arms != 0 && (gap_arms & (text_cell | arm_up | arm_down)) == 0;
        if (horizontal_gap && (_line_arms[index - 2] & text_cell) != 0)
        {
            _line_arms[index - 1] = 0;
            _screen[index - 1].ch = blank_char;
        }
    }

    if (width > 1 && this->_in_clip(x + 1, y, clip))
    {
        TCell & tail = this->_cell(x + 1, y);
        tail.ch = 0;
        tail.wide_tail = true;
        _line_arms[index + 1] = text_cell;
        _braille[index + 1] = 0;
    }
}

void ImguiRendererNcurses::_draw_fill(const ImDrawList *dl,
                                      const ImDrawCmd & cmd,
                                      const Primitive & prim,
                                      const ImVec4 & clip)
{
    const ImVec2 min = clamp_to_clip(prim.min, clip);
    const ImVec2 max = clamp_to_clip(prim.max, clip);
    if (prim.rect)
    {
        const RectAxis columns {.min = prim.min.x,
                                .max = prim.max.x,
                                .across = prim.max.y - prim.min.y,
                                .clip_min = clip.x,
                                .clip_max = clip.z,
                                .screen = _screen_w};
        const RectAxis rows {.min = prim.min.y,
                             .max = prim.max.y,
                             .across = prim.max.x - prim.min.x,
                             .clip_min = clip.y,
                             .clip_max = clip.w,
                             .screen = _screen_h};
        const auto [x0, x1] = covered_cells(columns, floor_cell(min.x), (int)std::ceil(max.x));
        const auto [y0, y1] = covered_cells(rows, floor_cell(min.y), (int)std::ceil(max.y));
        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
                this->_cover_cell(this->_index(x, y), prim.col);
        }
        return;
    }
    for (int y = floor_cell(min.y); y < (int)std::ceil(max.y); ++y)
    {
        for (int x = floor_cell(min.x); x < (int)std::ceil(max.x); ++x)
        {
            if (const std::optional<ImU32> col = this->_sample_primitive(dl, cmd, prim, x, y))
                this->_paint_cell(x, y, *col, clip);
        }
    }
}

void ImguiRendererNcurses::_draw_shaded_fill(const ImDrawList *dl,
                                             const ImDrawCmd & cmd,
                                             const Primitive & prim,
                                             uint32_t ch,
                                             ImU32 ch_col,
                                             const ImVec4 & clip)
{
    const ImVec2 min = clamp_to_clip(prim.min, clip);
    const ImVec2 max = clamp_to_clip(prim.max, clip);
    for (int y = floor_cell(min.y); y < (int)std::ceil(max.y); ++y)
    {
        for (int x = floor_cell(min.x); x < (int)std::ceil(max.x); ++x)
        {
            const std::optional<ImU32> col = this->_sample_primitive(dl, cmd, prim, x, y);
            if (!col || !this->_in_clip(x, y, clip))
                continue;
            this->_paint_cell(x, y, *col, clip);
            TCell & cell = this->_cell(x, y);
            cell.ch = ch;
            cell.fg = this->_blend(cell.bg, ch_col);
        }
    }
}

void ImguiRendererNcurses::_paint_cell(int x, int y, ImU32 col, const ImVec4 & clip)
{
    if (this->_in_clip(x, y, clip))
        this->_cover_cell(this->_index(x, y), col);
}

void ImguiRendererNcurses::_cover_cell(size_t index, ImU32 col)
{
    TCell & cell = _screen[index];
    cell.bg = this->_blend(this->_cell_bg(cell), col);
    cell.has_bg = true;
    if ((col >> IM_COL32_A_SHIFT) >= opaque_alpha)
    {
        cell = TCell {.ch = blank_char, .fg = cell.fg, .bg = cell.bg, .has_bg = true};
        _line_arms[index] = 0;
        _braille[index] = 0;
    }
}

void ImguiRendererNcurses::_draw_grab(const ImDrawList *dl,
                                      const ImDrawCmd & cmd,
                                      const Primitive & prim,
                                      const ImVec4 & clip)
{
    if (prim.vertex_count != 4)
    {
        this->_draw_fill(dl, cmd, prim, clip);
        return;
    }
    // the hardcoded 2-unit grab padding inverts the grab rect across a one-cell slider: keep its centre cell
    const ImVec2 & from = dl->VtxBuffer[(int)(cmd.VtxOffset + prim.first_vertex)].pos;
    const ImVec2 & to = dl->VtxBuffer[(int)(cmd.VtxOffset + prim.first_vertex + 2)].pos;
    const auto [x0, x1] = grab_span(from.x, to.x);
    const auto [y0, y1] = grab_span(from.y, to.y);
    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
            this->_paint_cell(x, y, prim.col, clip);
    }
}

void ImguiRendererNcurses::_draw_tab(const ImDrawList *dl,
                                     const ImDrawCmd & cmd,
                                     const Primitive & prim,
                                     const ImVec4 & clip)
{
    if (prim.max.y - prim.min.y > vertex_epsilon)
    {
        this->_draw_fill(dl, cmd, prim, clip);
        return;
    }
    // a one-row tab background starts one unit below the tab top: it collapses onto the tab bottom edge
    const int y = floor_cell(prim.min.y - 0.5f);
    for (int x = round_cell(prim.min.x); x < round_cell(prim.max.x); ++x)
        this->_paint_cell(x, y, prim.col, clip);
}

void ImguiRendererNcurses::_draw_histogram(const ImDrawList *dl,
                                           const ImDrawCmd & cmd,
                                           const Primitive & prim,
                                           const ImVec4 & clip)
{
    // a progress bar fills one whole frame row, a histogram bar rises from the plot bottom to any height
    const bool one_row = std::abs(prim.max.y - prim.min.y - 1.0f) < vertex_epsilon;
    const bool row_aligned = std::abs(prim.min.y - std::round(prim.min.y)) < vertex_epsilon;
    if (prim.vertex_count != 4)
        this->_draw_fill(dl, cmd, prim, clip);
    else if (one_row && row_aligned)
        this->_draw_progress_bar(prim, clip);
    else
        this->_draw_histogram_bar(prim, clip);
}

void ImguiRendererNcurses::_draw_progress_bar(const Primitive & prim, const ImVec4 & clip)
{
    const int y = round_cell(prim.min.y);
    const int x0 = round_cell(prim.min.x);
    const int x1 = floor_cell(prim.max.x + vertex_epsilon);
    for (int x = x0; x < x1; ++x)
        this->_put_block(x, y, 8, left_blocks, prim.col, clip);
    this->_put_block(x1, y, eighths_of(prim.max.x - (float)x1), left_blocks, prim.col, clip);
}

void ImguiRendererNcurses::_draw_histogram_bar(const Primitive & prim, const ImVec4 & clip)
{
    const int x0 = round_cell(prim.min.x);
    const int x1 = std::max(x0 + 1, round_cell(prim.max.x));
    const int top = (int)std::ceil(prim.min.y - vertex_epsilon);
    const int bottom = std::max(top, round_cell(prim.max.y));
    const uint8_t partial = eighths_of((float)top - prim.min.y);
    for (int x = x0; x < x1; ++x)
    {
        this->_put_block(x, top - 1, partial, lower_blocks, prim.col, clip);
        for (int y = top; y < bottom; ++y)
            this->_put_block(x, y, 8, lower_blocks, prim.col, clip);
    }
}

void ImguiRendererNcurses::_put_block(int x,
                                      int y,
                                      uint8_t eighths,
                                      const std::array<uint32_t, 8> & blocks,
                                      ImU32 col,
                                      const ImVec4 & clip)
{
    if (eighths == 0 || !this->_in_clip(x, y, clip))
        return;
    TCell & cell = this->_cell(x, y);
    if (eighths == 8 || (!_unicode && eighths >= 4))
    {
        cell.bg = rgb_of(col);
        cell.has_bg = true;
        cell.ch = blank_char;
    }
    else if (_unicode)
    {
        cell.ch = blocks[eighths];
        cell.fg = rgb_of(col);
    }
}

void ImguiRendererNcurses::_draw_line(const ImDrawList *dl,
                                      const ImDrawCmd & cmd,
                                      const Primitive & prim,
                                      bool table,
                                      const ImVec4 & clip)
{
    const float width = prim.max.x - prim.min.x;
    const float height = prim.max.y - prim.min.y;
    // a zero-length AddLine collapses onto its end point, shifted by half a unit in legacy stroke mode
    const float shift = (width < vertex_epsilon && height < vertex_epsilon) ? legacy_line_offset : 0.0f;
    const ImVec2 min = clamp_to_clip(ImVec2(prim.min.x - shift, prim.min.y - shift), clip);
    const ImVec2 max = clamp_to_clip(ImVec2(prim.max.x - shift, prim.max.y - shift), clip);
    const int x0 = floor_cell(min.x + vertex_epsilon);
    const int y0 = floor_cell(min.y + vertex_epsilon);
    const int x1 = std::max(x0, floor_cell(max.x + vertex_epsilon) - 1);
    const int y1 = std::max(y0, floor_cell(max.y + vertex_epsilon) - 1);

    // an outline too small to have an inside comes as a filled rect: both are drawn as a box
    if (width > 1.5f && height > 1.5f)
    {
        if (table)
            _table_border_rows.push_back({x0, x1, y0});
        else
        {
            this->_add_horizontal_line(x0, x1, y0, prim.col, clip);
            this->_add_horizontal_line(x0, x1, y1, prim.col, clip);
        }
        this->_add_vertical_line(x0, y0, y1, prim.col, clip);
        this->_add_vertical_line(x1, y0, y1, prim.col, clip);
        return;
    }

    const std::optional<bool> stroke_vertical = this->_stroke_is_vertical(dl, cmd, prim);
    const bool horizontal = stroke_vertical ? !*stroke_vertical : (table ? width > height : width >= height);
    if (horizontal && table)
        _table_border_rows.push_back({x0, x1, y0});
    else if (horizontal)
        this->_add_horizontal_line(x0, x1, y0, prim.col, clip);
    else
        this->_add_vertical_line(x0, y0, y1, prim.col, clip);
    if (!horizontal && table)
        _table_border_tops.push_back({x0, y0, prim.col});
}

std::optional<bool>
    ImguiRendererNcurses::_stroke_is_vertical(const ImDrawList *dl, const ImDrawCmd & cmd, const Primitive & prim) const
{
    // an AddLine quad straddles each end with a vertex pair (0-1, 2-3): a line shorter than its thickness keeps its direction
    if (prim.vertex_count != 4)
        return std::nullopt;
    const ImVec2 & v0 = dl->VtxBuffer[(int)(cmd.VtxOffset + prim.first_vertex)].pos;
    const ImVec2 & v1 = dl->VtxBuffer[(int)(cmd.VtxOffset + prim.first_vertex + 1)].pos;
    const ImVec2 & v2 = dl->VtxBuffer[(int)(cmd.VtxOffset + prim.first_vertex + 2)].pos;
    const ImVec2 & v3 = dl->VtxBuffer[(int)(cmd.VtxOffset + prim.first_vertex + 3)].pos;
    const float cap = std::abs(v1.x - v0.x) + std::abs(v1.y - v0.y);
    const float dx = (v2.x + v3.x - v0.x - v1.x) * 0.5f;
    const float dy = (v2.y + v3.y - v0.y - v1.y) * 0.5f;
    if (cap > 1.5f || (std::abs(dx) < vertex_epsilon && std::abs(dy) < vertex_epsilon))
        return std::nullopt;
    return std::abs(dy) > std::abs(dx);
}

void ImguiRendererNcurses::_join_table_borders(const ImVec4 & clip)
{
    // an outer top border pushes column borders one row down, off the first cells (NoHostExtendX: one column right)
    for (const TableBorderTop & top : _table_border_tops)
    {
        const bool below_border = std::ranges::any_of(_table_border_rows, [&](const TableBorderRow & row) {
            return row.y == top.y - 1 && row.x0 <= top.x && top.x <= row.x1 + 1;
        });
        if (!below_border)
            continue;
        this->_add_line_arms(top.x, top.y - 1, arm_down, top.col, clip);
        this->_add_line_arms(top.x, top.y, arm_up, top.col, clip);
    }
    _table_border_rows.clear();
    _table_border_tops.clear();
}

void ImguiRendererNcurses::_draw_separator(const ImDrawList *dl,
                                           const ImDrawCmd & cmd,
                                           const Primitive & prim,
                                           const ImVec4 & clip)
{
    // Separator() fills a rect; SeparatorText() strokes an AddLine at trunc(middle) + 1, the row below its label
    const bool horizontal_stroke = prim.vertex_count == 4 && prim.max.x - prim.min.x > prim.max.y - prim.min.y
                                   && dl->VtxBuffer[(int)(cmd.VtxOffset + prim.first_vertex)].pos.x
                                          == dl->VtxBuffer[(int)(cmd.VtxOffset + prim.first_vertex + 1)].pos.x;
    if (!horizontal_stroke)
    {
        this->_draw_line(dl, cmd, prim, false, clip);
        return;
    }
    Primitive label_row = prim;
    label_row.min.y -= 1.0f;
    label_row.max.y -= 1.0f;
    this->_draw_line(dl, cmd, label_row, false, clip);
}

float ImguiRendererNcurses::_ring_thickness(const ImDrawList *dl, const ImDrawCmd & cmd, const Primitive & prim) const
{
    // the inner vertices of a stroked ring sit one stroke thickness inside its outer edge
    ImVec2 thickness(FLT_MAX, FLT_MAX);
    for (unsigned int index = prim.first_vertex; index < prim.first_vertex + prim.vertex_count; ++index)
    {
        const ImVec2 & pos = dl->VtxBuffer[(int)(cmd.VtxOffset + index)].pos;
        if (pos.x - prim.min.x > vertex_epsilon)
            thickness.x = std::min(thickness.x, pos.x - prim.min.x);
        if (pos.y - prim.min.y > vertex_epsilon)
            thickness.y = std::min(thickness.y, pos.y - prim.min.y);
    }
    return std::max(thickness.x, thickness.y);
}

void ImguiRendererNcurses::_add_horizontal_line(int x0, int x1, int y, ImU32 col, const ImVec4 & clip)
{
    for (int x = x0; x <= x1; ++x)
        this->_add_line_arms(x, y, span_arms(x, x0, x1, arm_left, arm_right), col, clip);
}

void ImguiRendererNcurses::_add_vertical_line(int x, int y0, int y1, ImU32 col, const ImVec4 & clip)
{
    for (int y = y0; y <= y1; ++y)
        this->_add_line_arms(x, y, span_arms(y, y0, y1, arm_up, arm_down), col, clip);
}

void ImguiRendererNcurses::_draw_mark(int x, int y, uint32_t ch, ImU32 col, const ImVec4 & clip)
{
    if (!this->_in_clip(x, y, clip))
        return;
    TCell & cell = this->_cell(x, y);
    const size_t index = this->_index(x, y);
    cell.ch = ch;
    cell.fg = this->_blend(this->_cell_bg(cell), col);
    cell.wide_tail = false;
    _line_arms[index] = 0;
    _braille[index] = 0;
}

unsigned int ImguiRendererNcurses::_draw_text_mark(const ImDrawList *dl,
                                                   const ImDrawCmd & cmd,
                                                   const Primitive & first,
                                                   const ImVec4 & clip)
{
    _mark_run.assign(1, first);
    if (this->_draw_mark_run(dl, cmd, clip))
        return first.end;
    // a close cross is two separate lines: pair this primitive with the next one of its colour
    const unsigned int next = first.end;
    if (next + 2 < cmd.ElemCount && vertex_at(dl, cmd, next).col == first.col
        && this->_is_untextured(vertex_at(dl, cmd, next)))
    {
        _mark_run.push_back(this->_collect_primitive(dl, cmd, next));
        if (this->_draw_mark_run(dl, cmd, clip))
            return _mark_run.back().end;
    }
    this->_draw_fill(dl, cmd, first, clip);
    return first.end;
}

bool ImguiRendererNcurses::_draw_mark_run(const ImDrawList *dl, const ImDrawCmd & cmd, const ImVec4 & clip)
{
    _mark_points.clear();
    ImVec2 min(FLT_MAX, FLT_MAX);
    ImVec2 max(-FLT_MAX, -FLT_MAX);
    for (const Primitive & prim : _mark_run)
    {
        for (unsigned int index = prim.first; index < prim.end; ++index)
            _mark_points.push_back(vertex_at(dl, cmd, index).pos);
        min = ImMin(min, prim.min);
        max = ImMax(max, prim.max);
    }

    const uint32_t mark = this->_mark_char(ImVec2(max.x - min.x, max.y - min.y));
    if (mark == 0)
        return false;
    ImVec2 at((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    if (_mark_run.size() == 2)
        at = ImVec2(at.x - legacy_line_offset, at.y - legacy_line_offset);
    if (_mark_points.size() == 3)
    {
        at = ImVec2((_mark_points[0].x + _mark_points[1].x + _mark_points[2].x) / 3.0f,
                    (_mark_points[0].y + _mark_points[1].y + _mark_points[2].y) / 3.0f);
    }
    this->_draw_mark(floor_cell(at.x), floor_cell(at.y), mark, _mark_run.front().col, clip);
    return true;
}

uint32_t ImguiRendererNcurses::_mark_char(const ImVec2 & size) const
{
    if (std::max(size.x, size.y) > mark_max_cells)
        return 0;
    if (_mark_run.size() == 2)
    {
        const bool cross = _mark_points.size() == cross_pattern_vertex_count && is_cross_pattern(_mark_points.data());
        return cross ? this->_pick(cross_char, 'x') : 0;
    }
    if (_mark_run.size() != 1)
        return 0;
    const size_t triangle_count = _mark_points.size() / 3;
    if (triangle_count == 1)
        return this->_arrow_char(classify_arrow(_mark_points[0], _mark_points[1], _mark_points[2]));
    if (triangle_count == 4)
        return this->_pick(check_char, 'x');
    if (triangle_count >= 5)
        return this->_pick(bullet_char, '*');
    return 0;
}

uint32_t ImguiRendererNcurses::_arrow_char(uint32_t ascii_arrow) const
{
    switch (ascii_arrow)
    {
        case '>':
            return this->_pick(arrow_right_char, '>');
        case '<':
            return this->_pick(arrow_left_char, '<');
        case 'v':
            return this->_pick(arrow_down_char, 'v');
        case '^':
            return this->_pick(arrow_up_char, '^');
        default:
            return 0;
    }
}

uint32_t ImguiRendererNcurses::_pick(uint32_t unicode_ch, uint32_t ascii_ch) const
{
    return _unicode ? unicode_ch : ascii_ch;
}

bool ImguiRendererNcurses::_is_untextured(const ImDrawVert & vertex) const
{
    return vertex.uv.x == _white_uv.x && vertex.uv.y == _white_uv.y;
}

void ImguiRendererNcurses::_draw_plot_line(const ImDrawList *dl,
                                           const ImDrawCmd & cmd,
                                           const Primitive & prim,
                                           const ImVec4 & clip)
{
    ImVec2 from((prim.min.x + prim.max.x) * 0.5f, (prim.min.y + prim.max.y) * 0.5f);
    ImVec2 to = from;
    if (prim.vertex_count == 4)
    {
        // AddLine quad = p1-n, p1+n, p2-n, p2+n: each vertex pair straddles one end of the line
        const ImVec2 & start_a = dl->VtxBuffer[(int)(cmd.VtxOffset + prim.first_vertex)].pos;
        const ImVec2 & start_b = dl->VtxBuffer[(int)(cmd.VtxOffset + prim.first_vertex + 1)].pos;
        const ImVec2 & end_a = dl->VtxBuffer[(int)(cmd.VtxOffset + prim.first_vertex + 2)].pos;
        const ImVec2 & end_b = dl->VtxBuffer[(int)(cmd.VtxOffset + prim.first_vertex + 3)].pos;
        from = ImVec2((start_a.x + start_b.x) * 0.5f - legacy_line_offset,
                      (start_a.y + start_b.y) * 0.5f - legacy_line_offset);
        to = ImVec2((end_a.x + end_b.x) * 0.5f - legacy_line_offset, (end_a.y + end_b.y) * 0.5f - legacy_line_offset);
    }

    const float dot_dx = (to.x - from.x) * 2.0f;
    const float dot_dy = (to.y - from.y) * 4.0f;
    const int steps = std::max(1, (int)std::ceil(std::max(std::abs(dot_dx), std::abs(dot_dy))));
    for (int step = 0; step <= steps; ++step)
    {
        const float progress = (float)step / (float)steps;
        this->_add_braille_dot(from.x + (to.x - from.x) * progress,
                               from.y + (to.y - from.y) * progress,
                               prim.col,
                               clip);
    }
}

void ImguiRendererNcurses::_set_attribute(const Primitive & prim, Role role, const ImVec4 & clip)
{
    ImVec2 min = prim.min;
    ImVec2 max = prim.max;
    if (role == Role::nav)
    {
        const ImRect item = nav_item_rect();
        min = ImMax(min, item.Min);
        max = ImMin(max, item.Max);
    }

    int x0 = round_cell(min.x);
    int x1 = round_cell(max.x);
    int y0 = round_cell(min.y);
    int y1 = round_cell(max.y);
    if (role == Role::underline || role == Role::link_underline)
    {
        x0 = floor_cell(min.x + vertex_epsilon);
        x1 = floor_cell(max.x + vertex_epsilon);
        y0 = floor_cell(min.y + vertex_epsilon);
        // a link underline lies right below its text: at one cell per text row it marks that row
        if (role == Role::link_underline)
            --y0;
        y1 = y0 + 1;
    }

    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            if (!this->_in_clip(x, y, clip))
                continue;
            TCell & cell = this->_cell(x, y);
            if (role == Role::nav || role == Role::reverse)
                cell.reverse = true;
            else if (role == Role::dim)
                cell.dim = true;
            else
                cell.underline = true;
        }
    }
}

void ImguiRendererNcurses::_add_line_arms(int x, int y, uint8_t arms, ImU32 col, const ImVec4 & clip)
{
    if (!this->_in_clip(x, y, clip))
        return;
    TCell & cell = this->_cell(x, y);
    const size_t index = this->_index(x, y);
    if ((_line_arms[index] & text_cell) != 0 || (_line_arms[index] == 0 && cell.ch != 0 && cell.ch != blank_char))
        return;
    _line_arms[index] = (uint8_t)(_line_arms[index] | arms);
    cell.ch = this->_box_char(_line_arms[index]);
    cell.fg = this->_blend(this->_cell_bg(cell), col);
    cell.wide_tail = false;
}

void ImguiRendererNcurses::_add_braille_dot(float x, float y, ImU32 col, const ImVec4 & clip)
{
    const int cell_x = floor_cell(x);
    const int cell_y = floor_cell(y);
    if (!this->_in_clip(cell_x, cell_y, clip))
        return;
    TCell & cell = this->_cell(cell_x, cell_y);
    const size_t index = this->_index(cell_x, cell_y);
    if (_braille[index] == 0 && cell.ch != 0 && cell.ch != blank_char)
        return;
    const int dot_x = std::clamp((int)((x - (float)cell_x) * 2.0f), 0, 1);
    const int dot_y = std::clamp((int)((y - (float)cell_y) * 4.0f), 0, 3);
    _braille[index] = (uint8_t)(_braille[index] | braille_bits[(size_t)dot_x][(size_t)dot_y]);
    cell.ch = this->_pick(braille_base + _braille[index], '*');
    cell.fg = this->_blend(this->_cell_bg(cell), col);
    cell.wide_tail = false;
}

bool ImguiRendererNcurses::_in_clip(int x, int y, const ImVec4 & clip) const
{
    const float cx = (float)x + 0.5f;
    const float cy = (float)y + 0.5f;
    return cx >= clip.x && cx < clip.z && cy >= clip.y && cy < clip.w && x >= 0 && x < _screen_w && y >= 0
           && y < _screen_h;
}

TCell & ImguiRendererNcurses::_cell(int x, int y)
{
    return _screen[this->_index(x, y)];
}

uint32_t ImguiRendererNcurses::_blend(uint32_t under, ImU32 col) const
{
    const int alpha = (int)(col >> IM_COL32_A_SHIFT);
    if (alpha == 255)
        return rgb_of(col);
    uint32_t out = 0;
    for (int shift = 0; shift < IM_COL32_A_SHIFT; shift += 8)
    {
        const int below = channel(under, shift);
        const int above = channel(col, shift);
        out |= (uint32_t)(below + (above - below) * alpha / 255) << shift;
    }
    return out;
}

uint32_t ImguiRendererNcurses::_cell_bg(const TCell & cell) const
{
    return cell.has_bg ? cell.bg : _clear_rgb;
}

uint32_t ImguiRendererNcurses::_box_char(uint8_t arms) const
{
    if (_unicode)
        return box_chars[arms & 0xF];
    const bool vertical = (arms & (arm_up | arm_down)) != 0;
    const bool horizontal = (arms & (arm_left | arm_right)) != 0;
    if (vertical && horizontal)
        return '+';
    return vertical ? '|' : '-';
}

int ImguiRendererNcurses::_color_index(uint32_t rgb) const
{
    const int red = channel(rgb, IM_COL32_R_SHIFT);
    const int green = channel(rgb, IM_COL32_G_SHIFT);
    const int blue = channel(rgb, IM_COL32_B_SHIFT);
    // direct-colour terminfo (xterm-direct): the colour number is the 0xRRGGBB value itself
    if (_max_colors > 256)
        return (red << 16) | (green << 8) | blue;
    if (_max_colors >= 256)
        return rgb_to_ansi256((uint8_t)red, (uint8_t)green, (uint8_t)blue);
    return nearest_base_color(rgb, _max_colors >= 16 ? 16 : 8);
}

wchar_t ImguiRendererNcurses::_cell_char(int x, int y) const
{
    const TCell & cell = _screen[this->_index(x, y)];
    const bool printable = cell.ch >= 0x20 && cell.ch != 0x7F && (cell.ch < 0x80 || cell.ch >= 0xA0);
    const bool encodable = cell.ch <= 0x7E || _unicode;
    if (!printable || !encodable || cell.wide_tail)
        return L' ';
    const wchar_t out = (wchar_t)cell.ch;
    const bool tail_kept = x + 1 < _screen_w && _screen[this->_index(x + 1, y)].wide_tail;
    return (wcwidth(out) > 1 && !tail_kept) ? L' ' : out;
}

void ImguiRendererNcurses::_flush_row(int y)
{
    move(y, 0);

    int count = 0;
    int run_fg = -2;
    int run_bg = -2;
    attr_t run_attrs = 0;
    bool skip_tail = false;
    for (int x = 0; x < _screen_w; ++x)
    {
        // ncurses already advanced over the right half of the wide glyph
        if (std::exchange(skip_tail, false))
            continue;

        const TCell & cell = _screen[this->_index(x, y)];
        const bool blank = cell.ch == 0 || cell.ch == blank_char || cell.wide_tail;
        const int bg = cell.has_bg ? this->_color_index(cell.bg) : -1;
        const int fg = (blank && bg == run_bg) ? run_fg : this->_color_index(cell.fg);
        const attr_t attrs = (cell.dim ? A_DIM : A_NORMAL) | (cell.reverse ? A_REVERSE : A_NORMAL)
                             | (cell.underline ? A_UNDERLINE : A_NORMAL);
        if (fg != run_fg || bg != run_bg || attrs != run_attrs)
        {
            addnwstr(_row_buf.data(), count);
            count = 0;
            int pair = _max_colors > 0 ? std::max(0, alloc_pair(fg, bg)) : 0;
            attr_set(attrs, 0, &pair);
            run_fg = fg;
            run_bg = bg;
            run_attrs = attrs;
        }

        const wchar_t out = this->_cell_char(x, y);
        skip_tail = wcwidth(out) > 1;
        _row_buf[(size_t)count++] = out;
    }
    addnwstr(_row_buf.data(), count);
}

void ImguiRendererNcurses::_flush_to_ncurses()
{
    if (_screen == _flushed)
        return;
    for (int y = 0; y < _screen_h; ++y)
        this->_flush_row(y);
    _flushed = _screen;
}

void ImguiRendererNcurses::shutdown()
{
    _is_init = false;
    _rgb_roles.clear();
    _screen.clear();
    _flushed.clear();
    _line_arms.clear();
    _braille.clear();
}

bool ImguiRendererNcurses::init_headless(int w, int h)
{
    this->_setup_imgui();
    this->_resize_grid(w, h);
    _is_init = true;
    return true;
}

void ImguiRendererNcurses::set_unicode(bool unicode)
{
    _unicode = unicode;
    _flushed.clear();
}

const TCell & ImguiRendererNcurses::get_cell(int x, int y) const
{
    return _screen[this->_index(x, y)];
}

void ImguiRendererNcurses::clear_screen()
{
    std::ranges::fill(_screen, TCell {});
}

void ImguiRendererNcurses::rasterize_headless(ImDrawData *draw_data)
{
    this->_rasterize(draw_data);
}

} // namespace sihd::imgui
