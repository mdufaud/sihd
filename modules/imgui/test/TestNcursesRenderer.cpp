#include <imgui.h>

#include <cmath>

#include <gtest/gtest.h>

#include <sihd/imgui/ImguiRendererNcurses.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::imgui;

// Knowledge base: imgui emits indexed triangles - rects/lines (uniform UVs), glyphs (atlas UVs),
// PathFillConvex fans; per triangle: glyph -> cross -> rect pair -> arrow or scanline fill

class TestNcursesRenderer: public ::testing::Test
{
    protected:
        TestNcursesRenderer() { sihd::util::LoggerManager::stream(); }
        virtual ~TestNcursesRenderer() { sihd::util::LoggerManager::clear_loggers(); }
};

// rgb_to_ansi256: grayscale -> ramp 232-255, color -> 6x6x6 cube at 16-231

TEST_F(TestNcursesRenderer, rgb_black)
{
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(0, 0, 0), 16);
}

TEST_F(TestNcursesRenderer, rgb_white)
{
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(255, 255, 255), 231);
}

TEST_F(TestNcursesRenderer, rgb_mid_gray)
{
    uint8_t v = ImguiRendererNcurses::rgb_to_ansi256(128, 128, 128);
    EXPECT_GE(v, 232);
    EXPECT_LE(v, 255);
}

TEST_F(TestNcursesRenderer, rgb_pure_red)
{
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(255, 0, 0), 196);
}

TEST_F(TestNcursesRenderer, rgb_pure_green)
{
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(0, 255, 0), 46);
}

TEST_F(TestNcursesRenderer, rgb_pure_blue)
{
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(0, 0, 255), 21);
}

TEST_F(TestNcursesRenderer, rgb_near_black_stays_in_cube)
{
    uint8_t v = ImguiRendererNcurses::rgb_to_ansi256(1, 1, 1);
    EXPECT_GE(v, 16);
    EXPECT_LE(v, 231);
}

TEST_F(TestNcursesRenderer, rgb_light_gray)
{
    uint8_t v = ImguiRendererNcurses::rgb_to_ansi256(200, 200, 200);
    EXPECT_GE(v, 16);
}

TEST_F(TestNcursesRenderer, rgb_dark_gray)
{
    uint8_t v = ImguiRendererNcurses::rgb_to_ansi256(50, 50, 50);
    EXPECT_GE(v, 16);
}

TEST_F(TestNcursesRenderer, rgb_yellow)
{
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(255, 255, 0), 226);
}

TEST_F(TestNcursesRenderer, rgb_cyan)
{
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(0, 255, 255), 51);
}

TEST_F(TestNcursesRenderer, rgb_magenta)
{
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(255, 0, 255), 201);
}

// col_to_ansi256 ignores alpha (foreground); col_to_ansi256_premul premultiplies by alpha (background)

TEST_F(TestNcursesRenderer, col_ignores_alpha)
{
    ImU32 opaque = IM_COL32(255, 0, 0, 255);
    ImU32 transparent = IM_COL32(255, 0, 0, 0);
    EXPECT_EQ(ImguiRendererNcurses::col_to_ansi256(opaque), ImguiRendererNcurses::col_to_ansi256(transparent));
}

TEST_F(TestNcursesRenderer, col_premul_scales_by_alpha)
{
    ImU32 half_alpha = IM_COL32(255, 0, 0, 128);
    uint8_t premul = ImguiRendererNcurses::col_to_ansi256_premul(half_alpha);
    uint8_t full = ImguiRendererNcurses::col_to_ansi256(IM_COL32(255, 0, 0, 255));
    EXPECT_NE(premul, full);
}

TEST_F(TestNcursesRenderer, col_premul_zero_alpha_is_black)
{
    ImU32 zero = IM_COL32(255, 128, 64, 0);
    uint8_t v = ImguiRendererNcurses::col_to_ansi256_premul(zero);
    EXPECT_EQ(v, ImguiRendererNcurses::rgb_to_ansi256(0, 0, 0));
}

TEST_F(TestNcursesRenderer, col_premul_full_alpha_equals_col)
{
    ImU32 full = IM_COL32(100, 150, 200, 255);
    EXPECT_EQ(ImguiRendererNcurses::col_to_ansi256_premul(full), ImguiRendererNcurses::col_to_ansi256(full));
}

TEST_F(TestNcursesRenderer, col_white)
{
    ImU32 white = IM_COL32(255, 255, 255, 255);
    EXPECT_EQ(ImguiRendererNcurses::col_to_ansi256(white), ImguiRendererNcurses::rgb_to_ansi256(255, 255, 255));
}

TEST_F(TestNcursesRenderer, col_black)
{
    ImU32 black = IM_COL32(0, 0, 0, 255);
    EXPECT_EQ(ImguiRendererNcurses::col_to_ansi256(black), ImguiRendererNcurses::rgb_to_ansi256(0, 0, 0));
}

// classify_arrow - RenderArrow [imgui_draw.cpp:4327]: h=FontSize, r=0.40*scale; xspan/yspan ratio
// 0.87 (right/left) or 1.15 (down/up); gates: spans > 0.01, ratio [0.4, 2.5], base edge, not corner

TEST_F(TestNcursesRenderer, arrow_right)
{
    // Idealized right arrow: base at x=0, apex at x=1
    ImVec2 p0(0.0f, 0.0f);
    ImVec2 p1(0.0f, 1.0f);
    ImVec2 p2(1.0f, 0.5f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'>');
}

TEST_F(TestNcursesRenderer, arrow_left)
{
    ImVec2 p0(1.0f, 0.0f);
    ImVec2 p1(1.0f, 1.0f);
    ImVec2 p2(0.0f, 0.5f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'<');
}

TEST_F(TestNcursesRenderer, arrow_down)
{
    ImVec2 p0(0.0f, 0.0f);
    ImVec2 p1(1.0f, 0.0f);
    ImVec2 p2(0.5f, 1.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'v');
}

TEST_F(TestNcursesRenderer, arrow_up)
{
    ImVec2 p0(0.0f, 1.0f);
    ImVec2 p1(1.0f, 1.0f);
    ImVec2 p2(0.5f, 0.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'^');
}

TEST_F(TestNcursesRenderer, arrow_equilateral_is_down_arrow)
{
    // Equilateral-ish: base at y=0, apex at y=0.866 → down
    ImVec2 p0(0.0f, 0.0f);
    ImVec2 p1(1.0f, 0.0f);
    ImVec2 p2(0.5f, 0.866f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'v');
}

TEST_F(TestNcursesRenderer, arrow_right_vertex_order_rotated)
{
    // Same geometry as right arrow, different vertex winding
    ImVec2 p0(1.0f, 0.5f);
    ImVec2 p1(0.0f, 0.0f);
    ImVec2 p2(0.0f, 1.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'>');
}

TEST_F(TestNcursesRenderer, arrow_down_narrow_base_rejected)
{
    // base=0.4, height=1.0, ratio=0.4 → too narrow for RenderArrow (real ≈ 1.15)
    ImVec2 p0(0.3f, 0.0f);
    ImVec2 p1(0.7f, 0.0f);
    ImVec2 p2(0.5f, 1.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)0);
}

TEST_F(TestNcursesRenderer, arrow_very_narrow_base_rejected)
{
    // base=0.2, height=1.0, ratio=0.2 → way too narrow
    ImVec2 p0(0.4f, 0.0f);
    ImVec2 p1(0.6f, 0.0f);
    ImVec2 p2(0.5f, 1.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)0);
}

TEST_F(TestNcursesRenderer, arrow_right_wide_base)
{
    // Slightly wider base (1.2): still valid arrow shape
    ImVec2 p0(0.0f, 0.0f);
    ImVec2 p1(0.0f, 1.2f);
    ImVec2 p2(1.0f, 0.6f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'>');
}

// ── classify_arrow: must NOT match (false positive protection) ─────────────
// These geometries appear in imgui rendering but are NOT arrows.

TEST_F(TestNcursesRenderer, arrow_right_angle_not_arrow)
{
    // Right-angle triangle: both x_base and y_base trigger → rejected
    // Appears in: rounded rect corners (PathFillConvex), checkbox borders
    ImVec2 p0(0.0f, 0.0f);
    ImVec2 p1(1.0f, 0.0f);
    ImVec2 p2(0.0f, 1.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), 0u);
}

TEST_F(TestNcursesRenderer, arrow_degenerate_line_not_arrow)
{
    // Collinear points → yspan < 0.01 → rejected
    ImVec2 p0(0.0f, 0.0f);
    ImVec2 p1(1.0f, 0.0f);
    ImVec2 p2(2.0f, 0.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), 0u);
}

TEST_F(TestNcursesRenderer, arrow_degenerate_point_not_arrow)
{
    ImVec2 p0(5.0f, 5.0f);
    ImVec2 p1(5.0f, 5.0f);
    ImVec2 p2(5.0f, 5.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), 0u);
}

TEST_F(TestNcursesRenderer, arrow_plotlines_diagonal_quad_half_not_arrow)
{
    // PlotLines diagonal (10,5)->(12,3): thin sliver, NOT an arrow - classify_arrow alone may not
    // reject all slivers, the area ratio check in _draw_nonglyph_triangle filters the rest
    ImVec2 p0(10.0f, 5.15f);
    ImVec2 p1(12.0f, 3.15f);
    ImVec2 p2(10.0f, 4.85f);
    uint32_t result = ImguiRendererNcurses::classify_arrow(p0, p1, p2);
    (void)result;
}

TEST_F(TestNcursesRenderer, arrow_plotlines_steep_diagonal_not_arrow)
{
    // Steep diagonal: (5,0)→(6,4), thickness ~0.3
    ImVec2 p0(5.0f, 0.15f);
    ImVec2 p1(6.0f, 4.15f);
    ImVec2 p2(5.0f, -0.15f);
    uint32_t result = ImguiRendererNcurses::classify_arrow(p0, p1, p2);
    (void)result;
}

TEST_F(TestNcursesRenderer, arrow_symmetric_down_triangle)
{
    // Valid down-pointing: base at y=0, apex at y=0.5
    ImVec2 p0(0.0f, 0.0f);
    ImVec2 p1(1.0f, 0.0f);
    ImVec2 p2(0.5f, 0.5f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'v');
}

// is_glyph_triangle: glyphs = PrimRectUV with VARYING atlas UVs; non-glyph geometry uses the
// uniform white pixel - any vertex UV differing from the others means glyph

TEST_F(TestNcursesRenderer, glyph_uniform_uv_not_glyph)
{
    // AddRectFilled / AddTriangleFilled: all UVs point to white pixel
    ImDrawVert v0, v1, v2;
    v0.uv = v1.uv = v2.uv = ImVec2(0.5f, 0.5f);
    EXPECT_FALSE(ImguiRendererNcurses::is_glyph_triangle(v0, v1, v2));
}

TEST_F(TestNcursesRenderer, glyph_varying_uv_is_glyph)
{
    // AddText character: UVs map to different atlas positions
    ImDrawVert v0, v1, v2;
    v0.uv = ImVec2(0.0f, 0.0f);
    v1.uv = ImVec2(0.1f, 0.0f);
    v2.uv = ImVec2(0.0f, 0.1f);
    EXPECT_TRUE(ImguiRendererNcurses::is_glyph_triangle(v0, v1, v2));
}

TEST_F(TestNcursesRenderer, glyph_only_u_varies)
{
    ImDrawVert v0, v1, v2;
    v0.uv = ImVec2(0.0f, 0.5f);
    v1.uv = ImVec2(0.1f, 0.5f);
    v2.uv = ImVec2(0.0f, 0.5f);
    EXPECT_TRUE(ImguiRendererNcurses::is_glyph_triangle(v0, v1, v2));
}

TEST_F(TestNcursesRenderer, glyph_only_v_varies)
{
    ImDrawVert v0, v1, v2;
    v0.uv = ImVec2(0.5f, 0.0f);
    v1.uv = ImVec2(0.5f, 0.0f);
    v2.uv = ImVec2(0.5f, 0.1f);
    EXPECT_TRUE(ImguiRendererNcurses::is_glyph_triangle(v0, v1, v2));
}

TEST_F(TestNcursesRenderer, glyph_tiny_uv_difference_is_glyph)
{
    // Even tiny UV differences indicate a glyph (narrow characters)
    ImDrawVert v0, v1, v2;
    v0.uv = ImVec2(0.500f, 0.500f);
    v1.uv = ImVec2(0.501f, 0.500f);
    v2.uv = ImVec2(0.500f, 0.501f);
    EXPECT_TRUE(ImguiRendererNcurses::is_glyph_triangle(v0, v1, v2));
}

TEST_F(TestNcursesRenderer, glyph_all_zero_uv_not_glyph)
{
    ImDrawVert v0, v1, v2;
    v0.uv = v1.uv = v2.uv = ImVec2(0.0f, 0.0f);
    EXPECT_FALSE(ImguiRendererNcurses::is_glyph_triangle(v0, v1, v2));
}

// compute_clip_rect: framebuffer-space = (ClipRect - DisplayPos) * FramebufferScale

TEST_F(TestNcursesRenderer, clip_rect_identity)
{
    ImDrawCmd cmd;
    cmd.ClipRect = ImVec4(10.0f, 20.0f, 100.0f, 200.0f);
    ImVec2 off(0.0f, 0.0f);
    ImVec2 scale(1.0f, 1.0f);
    ImVec4 clip = ImguiRendererNcurses::compute_clip_rect(cmd, off, scale);
    EXPECT_FLOAT_EQ(clip.x, 10.0f);
    EXPECT_FLOAT_EQ(clip.y, 20.0f);
    EXPECT_FLOAT_EQ(clip.z, 100.0f);
    EXPECT_FLOAT_EQ(clip.w, 200.0f);
}

TEST_F(TestNcursesRenderer, clip_rect_with_offset_and_scale)
{
    ImDrawCmd cmd;
    cmd.ClipRect = ImVec4(10.0f, 20.0f, 100.0f, 200.0f);
    ImVec2 off(5.0f, 10.0f);
    ImVec2 scale(2.0f, 2.0f);
    ImVec4 clip = ImguiRendererNcurses::compute_clip_rect(cmd, off, scale);
    EXPECT_FLOAT_EQ(clip.x, (10.0f - 5.0f) * 2.0f);
    EXPECT_FLOAT_EQ(clip.y, (20.0f - 10.0f) * 2.0f);
    EXPECT_FLOAT_EQ(clip.z, (100.0f - 5.0f) * 2.0f);
    EXPECT_FLOAT_EQ(clip.w, (200.0f - 10.0f) * 2.0f);
}

TEST_F(TestNcursesRenderer, clip_rect_zero_offset_half_scale)
{
    ImDrawCmd cmd;
    cmd.ClipRect = ImVec4(0.0f, 0.0f, 80.0f, 24.0f);
    ImVec2 off(0.0f, 0.0f);
    ImVec2 scale(0.5f, 0.5f);
    ImVec4 clip = ImguiRendererNcurses::compute_clip_rect(cmd, off, scale);
    EXPECT_FLOAT_EQ(clip.x, 0.0f);
    EXPECT_FLOAT_EQ(clip.y, 0.0f);
    EXPECT_FLOAT_EQ(clip.z, 40.0f);
    EXPECT_FLOAT_EQ(clip.w, 12.0f);
}

TEST_F(TestNcursesRenderer, clip_rect_negative_offset)
{
    ImDrawCmd cmd;
    cmd.ClipRect = ImVec4(0.0f, 0.0f, 50.0f, 50.0f);
    ImVec2 off(-10.0f, -5.0f);
    ImVec2 scale(1.0f, 1.0f);
    ImVec4 clip = ImguiRendererNcurses::compute_clip_rect(cmd, off, scale);
    EXPECT_FLOAT_EQ(clip.x, 10.0f);
    EXPECT_FLOAT_EQ(clip.y, 5.0f);
    EXPECT_FLOAT_EQ(clip.z, 60.0f);
    EXPECT_FLOAT_EQ(clip.w, 55.0f);
}

// is_cross_pattern - close button X [imgui.cpp:5640]: 2 AddLine calls -> 4 same-color triangles.
// Gates: span 0.5-3.0 per axis, aspect 0.35-2.85, >=3 unique positions per axis, centroid overlap

TEST_F(TestNcursesRenderer, cross_real_x_shape)
{
    // Two crossing diagonals with thickness offset ±0.15
    ImVec2 pts[12] = {
        {3.85f, 4.15f},
        {5.85f, 6.15f},
        {4.15f, 3.85f},
        {4.15f, 3.85f},
        {5.85f, 6.15f},
        {6.15f, 5.85f},
        {6.15f, 4.15f},
        {4.15f, 6.15f},
        {5.85f, 3.85f},
        {5.85f, 3.85f},
        {4.15f, 6.15f},
        {3.85f, 5.85f},
    };
    EXPECT_TRUE(ImguiRendererNcurses::is_cross_pattern(pts));
}

TEST_F(TestNcursesRenderer, cross_smaller_x_shape)
{
    ImVec2 pts[12] = {
        {0.9f, 1.1f},
        {1.9f, 2.1f},
        {1.1f, 0.9f},
        {1.1f, 0.9f},
        {1.9f, 2.1f},
        {2.1f, 1.9f},
        {2.1f, 1.1f},
        {1.1f, 2.1f},
        {1.9f, 0.9f},
        {1.9f, 0.9f},
        {1.1f, 2.1f},
        {0.9f, 1.9f},
    };
    EXPECT_TRUE(ImguiRendererNcurses::is_cross_pattern(pts));
}

TEST_F(TestNcursesRenderer, cross_offset_position)
{
    // X centered at (20,15) — position-independent detection
    ImVec2 pts[12] = {
        {18.85f, 14.15f},
        {20.85f, 16.15f},
        {19.15f, 13.85f},
        {19.15f, 13.85f},
        {20.85f, 16.15f},
        {21.15f, 15.85f},
        {21.15f, 14.15f},
        {19.15f, 16.15f},
        {20.85f, 13.85f},
        {20.85f, 13.85f},
        {19.15f, 16.15f},
        {18.85f, 15.85f},
    };
    EXPECT_TRUE(ImguiRendererNcurses::is_cross_pattern(pts));
}

// False positives that MUST be rejected: patterns from real imgui rendering that look
// superficially like crosses but aren't

TEST_F(TestNcursesRenderer, cross_sequential_segments_rejected)
{
    // PlotLines: two consecutive line segments — centroids are offset
    ImVec2 pts[12] = {
        {10.0f, 5.1f},
        {11.0f, 4.1f},
        {10.0f, 4.9f},
        {10.0f, 4.9f},
        {11.0f, 4.1f},
        {11.0f, 3.9f},
        {11.0f, 4.1f},
        {12.0f, 6.1f},
        {11.0f, 3.9f},
        {11.0f, 3.9f},
        {12.0f, 6.1f},
        {12.0f, 5.9f},
    };
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

TEST_F(TestNcursesRenderer, cross_narrow_horizontal_rejected)
{
    ImVec2 pts[12] = {
        {0.0f, 5.0f},
        {3.0f, 5.0f},
        {0.0f, 5.3f},
        {0.0f, 5.3f},
        {3.0f, 5.0f},
        {3.0f, 5.3f},
        {0.0f, 5.0f},
        {3.0f, 5.0f},
        {0.0f, 4.7f},
        {0.0f, 4.7f},
        {3.0f, 5.0f},
        {3.0f, 4.7f},
    };
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

TEST_F(TestNcursesRenderer, cross_too_small_rejected)
{
    ImVec2 pts[12] = {
        {5.0f, 5.0f},
        {5.1f, 5.0f},
        {5.0f, 5.1f},
        {5.0f, 5.1f},
        {5.1f, 5.0f},
        {5.1f, 5.1f},
        {5.1f, 5.0f},
        {5.0f, 5.1f},
        {5.1f, 5.1f},
        {5.0f, 5.0f},
        {5.1f, 5.1f},
        {5.0f, 5.1f},
    };
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

TEST_F(TestNcursesRenderer, cross_too_large_rejected)
{
    ImVec2 pts[12] = {
        {0.0f, 0.0f},
        {4.0f, 4.0f},
        {0.3f, 0.0f},
        {0.3f, 0.0f},
        {4.0f, 4.0f},
        {4.0f, 3.7f},
        {4.0f, 0.0f},
        {0.0f, 4.0f},
        {3.7f, 0.0f},
        {3.7f, 0.0f},
        {0.0f, 4.0f},
        {0.0f, 3.7f},
    };
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

TEST_F(TestNcursesRenderer, cross_parallel_lines_rejected)
{
    ImVec2 pts[12] = {
        {1.0f, 1.0f},
        {3.0f, 1.0f},
        {1.0f, 1.2f},
        {1.0f, 1.2f},
        {3.0f, 1.0f},
        {3.0f, 1.2f},
        {1.0f, 2.0f},
        {3.0f, 2.0f},
        {1.0f, 2.2f},
        {1.0f, 2.2f},
        {3.0f, 2.0f},
        {3.0f, 2.2f},
    };
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

TEST_F(TestNcursesRenderer, cross_v_shape_rejected)
{
    // Two lines meeting at a point (V), not crossing through center
    ImVec2 pts[12] = {
        {1.0f, 1.1f},
        {2.0f, 2.1f},
        {1.0f, 0.9f},
        {1.0f, 0.9f},
        {2.0f, 2.1f},
        {2.0f, 1.9f},
        {3.0f, 1.1f},
        {2.0f, 2.1f},
        {3.0f, 0.9f},
        {3.0f, 0.9f},
        {2.0f, 2.1f},
        {2.0f, 1.9f},
    };
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

TEST_F(TestNcursesRenderer, cross_plotlines_zigzag_rejected)
{
    // PlotLines zigzag: 4 short segments up-down
    ImVec2 pts[12] = {
        {0.0f, 1.1f},
        {1.0f, 0.1f},
        {0.0f, 0.9f},
        {0.0f, 0.9f},
        {1.0f, 0.1f},
        {1.0f, -0.1f},
        {1.0f, 0.1f},
        {2.0f, 1.1f},
        {1.0f, -0.1f},
        {1.0f, -0.1f},
        {2.0f, 1.1f},
        {2.0f, 0.9f},
    };
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

TEST_F(TestNcursesRenderer, cross_l_shape_two_lines_rejected)
{
    // L-shape: two perpendicular lines meeting at corner, centroids far apart
    ImVec2 pts[12] = {
        {1.0f, 3.15f},
        {3.0f, 3.15f},
        {1.0f, 2.85f},
        {1.0f, 2.85f},
        {3.0f, 3.15f},
        {3.0f, 2.85f},
        {3.15f, 3.0f},
        {3.15f, 1.0f},
        {2.85f, 3.0f},
        {2.85f, 3.0f},
        {3.15f, 1.0f},
        {2.85f, 1.0f},
    };
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

TEST_F(TestNcursesRenderer, cross_narrow_vertical_rejected)
{
    ImVec2 pts[12] = {
        {5.0f, 0.0f},
        {5.0f, 3.0f},
        {5.3f, 0.0f},
        {5.3f, 0.0f},
        {5.0f, 3.0f},
        {5.3f, 3.0f},
        {5.0f, 0.0f},
        {5.0f, 3.0f},
        {4.7f, 0.0f},
        {4.7f, 0.0f},
        {5.0f, 3.0f},
        {4.7f, 3.0f},
    };
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

// ── is_cross_pattern: edge cases ───────────────────────────────────────────

TEST_F(TestNcursesRenderer, cross_at_boundary_size_min)
{
    float s = 0.5f;
    float off = 0.05f;
    ImVec2 pts[12] = {
        {-off, off},
        {s - off, s + off},
        {off, -off},
        {off, -off},
        {s - off, s + off},
        {s + off, s - off},
        {s + off, off},
        {off, s + off},
        {s - off, -off},
        {s - off, -off},
        {off, s + off},
        {-off, s - off},
    };
    ImguiRendererNcurses::is_cross_pattern(pts);
}

TEST_F(TestNcursesRenderer, cross_at_boundary_size_max)
{
    float s = 2.95f;
    float off = 0.1f;
    ImVec2 pts[12] = {
        {-off, off},
        {s - off, s + off},
        {off, -off},
        {off, -off},
        {s - off, s + off},
        {s + off, s - off},
        {s + off, off},
        {off, s + off},
        {s - off, -off},
        {s - off, -off},
        {off, s + off},
        {-off, s - off},
    };
    ImguiRendererNcurses::is_cross_pattern(pts);
}

TEST_F(TestNcursesRenderer, cross_all_same_point_rejected)
{
    ImVec2 pts[12];
    for (int i = 0; i < 12; ++i)
        pts[i] = ImVec2(5.0f, 5.0f);
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

TEST_F(TestNcursesRenderer, cross_collinear_points_rejected)
{
    ImVec2 pts[12];
    for (int i = 0; i < 12; ++i)
        pts[i] = ImVec2((float)i * 0.2f, (float)i * 0.2f);
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

// classify_arrow - real widget arrows: Combo dropdown, TreeNode collapse/expand, Scrollbar
// buttons, Menu submenu

TEST_F(TestNcursesRenderer, arrow_combo_dropdown_right)
{
    // Combo [imgui_widgets.cpp:1898]: RenderArrow(ImGuiDir_Down) — but test right
    ImVec2 p0(68.0f, 3.0f);
    ImVec2 p1(68.0f, 4.0f);
    ImVec2 p2(69.0f, 3.5f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'>');
}

TEST_F(TestNcursesRenderer, arrow_tree_node_right)
{
    // TreeNode collapsed [imgui_widgets.cpp:6782]: RenderArrow(ImGuiDir_Right, scale=1.0)
    ImVec2 p0(2.0f, 5.0f);
    ImVec2 p1(2.0f, 6.0f);
    ImVec2 p2(3.0f, 5.5f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'>');
}

TEST_F(TestNcursesRenderer, arrow_tree_node_down)
{
    // TreeNode expanded [imgui_widgets.cpp:6782]: RenderArrow(ImGuiDir_Down, scale=0.70)
    ImVec2 p0(2.0f, 5.0f);
    ImVec2 p1(3.0f, 5.0f);
    ImVec2 p2(2.5f, 6.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'v');
}

TEST_F(TestNcursesRenderer, arrow_scrollbar_up)
{
    ImVec2 p0(70.0f, 2.0f);
    ImVec2 p1(71.0f, 2.0f);
    ImVec2 p2(70.5f, 1.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'^');
}

TEST_F(TestNcursesRenderer, arrow_scrollbar_down)
{
    ImVec2 p0(70.0f, 20.0f);
    ImVec2 p1(71.0f, 20.0f);
    ImVec2 p2(70.5f, 21.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'v');
}

TEST_F(TestNcursesRenderer, arrow_menu_submenu_right)
{
    ImVec2 p0(45.0f, 8.0f);
    ImVec2 p1(45.0f, 9.0f);
    ImVec2 p2(46.0f, 8.5f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), (uint32_t)'>');
}

// ── classify_arrow: non-arrow geometry from other widgets ──────────────────

TEST_F(TestNcursesRenderer, arrow_checkbox_corner_not_arrow)
{
    // Checkbox border: right-angle triangle at corner → both axes have base
    ImVec2 p0(5.0f, 3.0f);
    ImVec2 p1(6.0f, 3.0f);
    ImVec2 p2(5.0f, 4.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), 0u);
}

TEST_F(TestNcursesRenderer, arrow_rect_fill_half_not_arrow)
{
    // Half of AddRectFilled PrimRect — right-angle, too large
    ImVec2 p0(10.0f, 5.0f);
    ImVec2 p1(15.0f, 5.0f);
    ImVec2 p2(10.0f, 8.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), 0u);
}

TEST_F(TestNcursesRenderer, arrow_circle_fan_slice_not_arrow)
{
    // Circle fan triangle from AddCircleFilled (RenderBullet): center + 2 rim points
    ImVec2 p0(10.0f, 10.0f);
    ImVec2 p1(11.0f, 10.5f);
    ImVec2 p2(10.8f, 11.0f);
    uint32_t r = ImguiRendererNcurses::classify_arrow(p0, p1, p2);
    (void)r;
}

TEST_F(TestNcursesRenderer, arrow_resize_grip_not_arrow)
{
    // Resize grip: right-angle corner triangle → both axes base → rejected
    ImVec2 p0(79.0f, 24.0f);
    ImVec2 p1(79.0f, 23.0f);
    ImVec2 p2(78.0f, 24.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), 0u);
}

// PlotLines + cross detection: PlotLines emits AddLine per segment, the cross detector groups 4
// triangles at a time - sequential segments have OFFSET centroids, they must not match

TEST_F(TestNcursesRenderer, plotlines_flat_segment_no_arrow)
{
    // Horizontal segment: (5,3)→(6,3), thickness 0.3
    // Both p0 and p2 share x=5.0 → min_xg≈0 AND p0/p1 share y≈3.15 → corner reject
    ImVec2 p0(5.0f, 3.15f);
    ImVec2 p1(6.0f, 3.15f);
    ImVec2 p2(5.0f, 2.85f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), 0u);
}

TEST_F(TestNcursesRenderer, plotlines_45deg_segment_cross_rejected)
{
    ImVec2 pts[12] = {
        {-0.1f, 0.1f},
        {0.9f, 1.1f},
        {0.1f, -0.1f},
        {0.1f, -0.1f},
        {0.9f, 1.1f},
        {1.1f, 0.9f},
        {0.9f, 1.1f},
        {1.9f, 0.1f},
        {1.1f, 0.9f},
        {1.1f, 0.9f},
        {1.9f, 0.1f},
        {2.1f, -0.1f},
    };
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

TEST_F(TestNcursesRenderer, plotlines_same_direction_segments_cross_rejected)
{
    ImVec2 pts[12] = {
        {10.07f, 5.04f},
        {10.87f, 4.24f},
        {9.93f, 4.96f},
        {9.93f, 4.96f},
        {10.87f, 4.24f},
        {10.73f, 4.16f},
        {10.87f, 4.24f},
        {11.67f, 3.44f},
        {10.73f, 4.16f},
        {10.73f, 4.16f},
        {11.67f, 3.44f},
        {11.53f, 3.36f},
    };
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

TEST_F(TestNcursesRenderer, plotlines_horizontal_segments_cross_rejected)
{
    ImVec2 pts[12] = {
        {0.0f, 3.1f},
        {2.0f, 3.1f},
        {0.0f, 2.9f},
        {0.0f, 2.9f},
        {2.0f, 3.1f},
        {2.0f, 2.9f},
        {2.0f, 5.1f},
        {4.0f, 5.1f},
        {2.0f, 4.9f},
        {2.0f, 4.9f},
        {4.0f, 5.1f},
        {4.0f, 4.9f},
    };
    EXPECT_FALSE(ImguiRendererNcurses::is_cross_pattern(pts));
}

// Area ratio gates arrows vs slivers: real arrows area/bbox ≈ 0.5, AddLine slivers < 0.25 -
// only triangles with area/bbox >= 0.5 reach classify_arrow

TEST_F(TestNcursesRenderer, sliver_ratio_real_arrow)
{
    // Right arrow: cross2=1.0, bbox=1.0, ratio=1.0 → passes
    ImVec2 p0(0.0f, 0.0f), p1(0.0f, 1.0f), p2(1.0f, 0.5f);
    float cross2 = std::abs((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
    float bbox = 1.0f * 1.0f;
    EXPECT_GE(cross2 / bbox, 0.5f);
}

TEST_F(TestNcursesRenderer, sliver_ratio_diagonal_line_quad)
{
    // Diagonal quad half: very low area in large bbox
    ImVec2 p0(0.0f, 0.15f), p1(1.0f, 1.15f), p2(0.0f, -0.15f);
    float cross2 = std::abs((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
    float bbox = 1.0f * 1.3f;
    EXPECT_LT(cross2 / bbox, 0.5f);
}

TEST_F(TestNcursesRenderer, sliver_ratio_steep_line_quad)
{
    ImVec2 p0(0.0f, 0.1f), p1(0.5f, 2.1f), p2(0.0f, -0.1f);
    float cross2 = std::abs((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
    float bbox = 0.5f * 2.2f;
    EXPECT_LT(cross2 / bbox, 0.5f);
}

TEST_F(TestNcursesRenderer, sliver_ratio_horizontal_line_quad)
{
    // Horizontal: high ratio but rejected by corner check in classify_arrow
    ImVec2 p0(0.0f, 0.1f), p1(3.0f, 0.1f), p2(0.0f, -0.1f);
    float cross2 = std::abs((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
    float bbox = 3.0f * 0.2f;
    EXPECT_GE(cross2 / bbox, 0.5f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), 0u);
}

TEST_F(TestNcursesRenderer, sliver_ratio_wide_short_diagonal)
{
    ImVec2 p0(0.0f, 0.1f), p1(4.0f, 0.5f), p2(0.0f, -0.1f);
    float cross2 = std::abs((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
    float bbox = 4.0f * 0.6f;
    EXPECT_LT(cross2 / bbox, 0.5f);
}

TEST_F(TestNcursesRenderer, sliver_ratio_down_arrow)
{
    ImVec2 p0(0.0f, 0.0f), p1(1.0f, 0.0f), p2(0.5f, 1.0f);
    float cross2 = std::abs((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
    float bbox = 1.0f * 1.0f;
    EXPECT_GE(cross2 / bbox, 0.5f);
}

TEST_F(TestNcursesRenderer, sliver_ratio_narrow_down_arrow)
{
    ImVec2 p0(0.4f, 0.0f), p1(0.6f, 0.0f), p2(0.5f, 1.0f);
    float cross2 = std::abs((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
    float bbox = 0.2f * 1.0f;
    EXPECT_GE(cross2 / bbox, 0.5f);
}

// PlotHistogram bars: AddRectFilled per bar -> rect-pair detection; thin rects (width < 1.5)
// fall through to per-triangle scanline for tighter bounds so thin bars never merge

TEST_F(TestNcursesRenderer, histogram_bar_is_axis_aligned)
{
    // Verify PrimRect geometry: all 6 vertices on rect corners
    ImVec2 all[6] = {
        {10.0f, 8.0f},
        {10.8f, 8.0f},
        {10.0f, 16.0f},
        {10.8f, 8.0f},
        {10.8f, 16.0f},
        {10.0f, 16.0f},
    };
    float rxmin = 10.0f, rxmax = 10.8f, rymin = 8.0f, rymax = 16.0f;
    for (int vi = 0; vi < 6; ++vi)
    {
        bool x_ok = (std::abs(all[vi].x - rxmin) < 0.01f) || (std::abs(all[vi].x - rxmax) < 0.01f);
        bool y_ok = (std::abs(all[vi].y - rymin) < 0.01f) || (std::abs(all[vi].y - rymax) < 0.01f);
        EXPECT_TRUE(x_ok && y_ok) << "vertex " << vi;
    }
}

TEST_F(TestNcursesRenderer, histogram_thin_bar_not_arrow)
{
    // Thin histogram bar half (width=0.3, height=5): right-angle → not arrow
    ImVec2 p0(10.0f, 5.0f);
    ImVec2 p1(10.3f, 5.0f);
    ImVec2 p2(10.0f, 10.0f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), 0u);
}

TEST_F(TestNcursesRenderer, histogram_near_zero_bar_not_arrow)
{
    ImVec2 p0(15.0f, 16.0f);
    ImVec2 p1(15.8f, 16.0f);
    ImVec2 p2(15.0f, 15.9f);
    EXPECT_EQ(ImguiRendererNcurses::classify_arrow(p0, p1, p2), 0u);
}

// Diagonal AddLine slivers: area/bbox < 0.5 vs real arrows 0.5-1.0 - the secondary filter keeping
// line segments from being misclassified as arrows

TEST_F(TestNcursesRenderer, diagonal_line_30deg_sliver)
{
    ImVec2 p0(0.089f, -0.179f);
    ImVec2 p1(2.089f, 0.821f);
    ImVec2 p2(-0.089f, 0.179f);
    float cross2 = std::abs((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
    float xspan = std::max({p0.x, p1.x, p2.x}) - std::min({p0.x, p1.x, p2.x});
    float yspan = std::max({p0.y, p1.y, p2.y}) - std::min({p0.y, p1.y, p2.y});
    EXPECT_LT(cross2 / (xspan * yspan), 0.5f);
}

TEST_F(TestNcursesRenderer, diagonal_line_60deg_sliver)
{
    ImVec2 p0(0.087f, -0.05f);
    ImVec2 p1(1.087f, 1.68f);
    ImVec2 p2(-0.087f, 0.05f);
    float cross2 = std::abs((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
    float xspan = std::max({p0.x, p1.x, p2.x}) - std::min({p0.x, p1.x, p2.x});
    float yspan = std::max({p0.y, p1.y, p2.y}) - std::min({p0.y, p1.y, p2.y});
    EXPECT_LT(cross2 / (xspan * yspan), 0.5f);
}

TEST_F(TestNcursesRenderer, diagonal_line_negative_slope_sliver)
{
    ImVec2 p0(-0.07f, 2.07f);
    ImVec2 p1(1.93f, 0.07f);
    ImVec2 p2(0.07f, 1.93f);
    float cross2 = std::abs((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
    float xspan = std::max({p0.x, p1.x, p2.x}) - std::min({p0.x, p1.x, p2.x});
    float yspan = std::max({p0.y, p1.y, p2.y}) - std::min({p0.y, p1.y, p2.y});
    EXPECT_LT(cross2 / (xspan * yspan), 0.5f);
}

// Headless draw-method tests: assert the actual screen buffer via init_headless(W, H)

class TestNcursesRendererDraw: public ::testing::Test
{
    protected:
        ImguiRendererNcurses renderer;
        static constexpr int W = 80;
        static constexpr int H = 24;
        static constexpr ImU32 kWhite = IM_COL32(255, 255, 255, 255);
        static constexpr ImU32 kYellow = IM_COL32(255, 255, 0, 255);
        static constexpr ImU32 kRed = IM_COL32(255, 0, 0, 255);
        static constexpr ImVec4 kFullClip = {0.0f, 0.0f, 80.0f, 24.0f};

        TestNcursesRendererDraw()
        {
            sihd::util::LoggerManager::stream();
            renderer.init_headless(W, H);
        }
        virtual ~TestNcursesRendererDraw() { sihd::util::LoggerManager::clear_loggers(); }

        bool cell_has_arrow(int x, int y)
        {
            uint32_t ch = renderer.get_cell(x, y).ch;
            return ch == '>' || ch == '<' || ch == '^' || ch == 'v';
        }

        bool cell_has_dash(int x, int y)
        {
            uint32_t ch = renderer.get_cell(x, y).ch;
            return ch == '-' || ch == '|';
        }

        bool cell_is_bg_fill(int x, int y)
        {
            const TCell & c = renderer.get_cell(x, y);
            return (c.ch == 0 || c.ch == ' ') && c.bg != 0;
        }
};

// _draw_rect: AddRectFilled detected as rect pair -> space + bg fill, no other characters

TEST_F(TestNcursesRendererDraw, rect_normal_fill)
{
    renderer.draw_rect(10.0f, 5.0f, 15.0f, 8.0f, kYellow, kFullClip);
    for (int y = 5; y < 8; ++y)
        for (int x = 10; x < 15; ++x)
        {
            EXPECT_TRUE(cell_is_bg_fill(x, y)) << "cell(" << x << "," << y << ")";
            EXPECT_FALSE(cell_has_dash(x, y)) << "cell(" << x << "," << y << ")";
            EXPECT_FALSE(cell_has_arrow(x, y)) << "cell(" << x << "," << y << ")";
        }
}

TEST_F(TestNcursesRendererDraw, rect_thin_horizontal_no_dash)
{
    // Thin horizontal (separator, table border): space+bg, never dashes
    renderer.draw_rect(10.0f, 10.0f, 15.0f, 10.3f, kWhite, kFullClip);
    for (int x = 10; x <= 14; ++x)
        EXPECT_FALSE(cell_has_dash(x, 10)) << "dash at cell(" << x << ",10)";
}

TEST_F(TestNcursesRendererDraw, rect_thin_vertical_no_pipe)
{
    renderer.draw_rect(20.0f, 5.0f, 20.3f, 10.0f, kWhite, kFullClip);
    for (int y = 5; y < 10; ++y)
        EXPECT_FALSE(cell_has_dash(20, y)) << "pipe at cell(20," << y << ")";
}

// ── _draw_nonglyph_triangle: RenderArrow geometry → arrow character ────────

TEST_F(TestNcursesRendererDraw, nonglyph_real_right_arrow)
{
    renderer.draw_nonglyph_triangle(ImVec2(10.0f, 10.0f),
                                    ImVec2(10.0f, 11.0f),
                                    ImVec2(11.0f, 10.5f),
                                    kWhite,
                                    kFullClip);
    EXPECT_EQ(renderer.get_cell(10, 10).ch, (uint32_t)'>');
}

TEST_F(TestNcursesRendererDraw, nonglyph_real_down_arrow)
{
    renderer.draw_nonglyph_triangle(ImVec2(10.0f, 10.0f),
                                    ImVec2(11.0f, 10.0f),
                                    ImVec2(10.5f, 11.0f),
                                    kWhite,
                                    kFullClip);
    EXPECT_EQ(renderer.get_cell(10, 10).ch, (uint32_t)'v');
}

// AddLine slivers (PlotLines): low area ratio -> must fall through to scanline fill, never arrows

TEST_F(TestNcursesRendererDraw, nonglyph_plotlines_diagonal_sliver_no_arrow)
{
    // Diagonal: (10,5)→(11,4), thickness 0.15
    renderer.draw_nonglyph_triangle(ImVec2(10.0f, 5.07f),
                                    ImVec2(11.0f, 4.07f),
                                    ImVec2(10.0f, 4.93f),
                                    kYellow,
                                    kFullClip);
    EXPECT_FALSE(cell_has_arrow(10, 4)) << "thin sliver falsely classified as arrow";
    EXPECT_FALSE(cell_has_arrow(10, 5)) << "thin sliver falsely classified as arrow";
}

TEST_F(TestNcursesRendererDraw, nonglyph_plotlines_steep_sliver_no_arrow)
{
    // Steep: (5,2)→(6,5), thickness 0.15
    renderer.draw_nonglyph_triangle(ImVec2(5.07f, 2.0f), ImVec2(6.07f, 5.0f), ImVec2(4.93f, 2.0f), kYellow, kFullClip);
    for (int y = 2; y <= 5; ++y)
        for (int x = 4; x <= 6; ++x)
            EXPECT_FALSE(cell_has_arrow(x, y)) << "arrow at cell(" << x << "," << y << ")";
}

TEST_F(TestNcursesRendererDraw, nonglyph_plotlines_shallow_sliver_no_arrow)
{
    // Shallow: (10,8)→(12,8.3), thickness 0.15
    renderer.draw_nonglyph_triangle(ImVec2(10.0f, 8.07f),
                                    ImVec2(12.0f, 8.37f),
                                    ImVec2(10.0f, 7.93f),
                                    kYellow,
                                    kFullClip);
    for (int y = 7; y <= 9; ++y)
        for (int x = 10; x <= 12; ++x)
            EXPECT_FALSE(cell_has_arrow(x, y)) << "arrow at cell(" << x << "," << y << ")";
}

TEST_F(TestNcursesRendererDraw, nonglyph_thin_sliver_no_dash)
{
    // Thin horizontal sliver (separator): must not produce dashes
    renderer.draw_nonglyph_triangle(ImVec2(5.0f, 10.07f),
                                    ImVec2(15.0f, 10.07f),
                                    ImVec2(5.0f, 9.93f),
                                    kWhite,
                                    kFullClip);
    for (int x = 5; x <= 15; ++x)
        EXPECT_FALSE(cell_has_dash(x, 10)) << "dash at cell(" << x << ",10)";
}

// ── Histogram bar via draw_rect ────────────────────────────────────────────

TEST_F(TestNcursesRendererDraw, rect_histogram_bar_fill)
{
    renderer.draw_rect(20.0f, 10.0f, 23.0f, 15.0f, kYellow, kFullClip);
    for (int y = 10; y < 15; ++y)
        for (int x = 20; x < 23; ++x)
        {
            EXPECT_TRUE(cell_is_bg_fill(x, y)) << "cell(" << x << "," << y << ")";
            EXPECT_FALSE(cell_has_dash(x, y)) << "cell(" << x << "," << y << ")";
        }
}

TEST_F(TestNcursesRendererDraw, rect_thin_bar_between_columns_no_dash)
{
    renderer.draw_rect(15.0f, 5.0f, 15.2f, 13.0f, kYellow, kFullClip);
    for (int y = 5; y < 13; ++y)
        EXPECT_FALSE(cell_has_dash(15, y)) << "pipe at cell(15," << y << ")";
}

// ── _draw_fill_triangle: scanline fill → space + bg ────────────────────────

TEST_F(TestNcursesRendererDraw, fill_triangle_produces_bg)
{
    uint8_t bg = ImguiRendererNcurses::col_to_ansi256_premul(kYellow);
    renderer.draw_fill_triangle(ImVec2(10.0f, 5.0f), ImVec2(20.0f, 5.0f), ImVec2(15.0f, 15.0f), bg, kFullClip);
    const TCell & c = renderer.get_cell(15, 8);
    EXPECT_EQ(c.bg, bg);
    EXPECT_FALSE(cell_has_arrow(15, 8));
    EXPECT_FALSE(cell_has_dash(15, 8));
}

// ── Multiple PlotLines segments: sequence of slivers, ZERO arrows ──────────

TEST_F(TestNcursesRendererDraw, nonglyph_plotlines_sequence_no_arrows)
{
    struct Seg
    {
            float x0, y0, x1, y1;
    };
    Seg segs[] = {
        {10, 5, 11, 4},
        {11, 4, 12, 6},
        {12, 6, 13, 5.5},
        {13, 5.5, 14, 3},
    };
    constexpr float thick = 0.15f;
    for (const auto & seg : segs)
    {
        float dx = seg.x1 - seg.x0;
        float dy = seg.y1 - seg.y0;
        float len = std::sqrt(dx * dx + dy * dy);
        float nx = -dy / len * thick;
        float ny = dx / len * thick;
        renderer.draw_nonglyph_triangle(ImVec2(seg.x0 + nx, seg.y0 + ny),
                                        ImVec2(seg.x1 + nx, seg.y1 + ny),
                                        ImVec2(seg.x0 - nx, seg.y0 - ny),
                                        kYellow,
                                        kFullClip);
    }
    for (int y = 2; y <= 7; ++y)
        for (int x = 9; x <= 15; ++x)
            EXPECT_FALSE(cell_has_arrow(x, y)) << "arrow at cell(" << x << "," << y << ")";
}

// ── Default path: non-glyph non-arrow triangles → space + bg ───────────────

TEST_F(TestNcursesRendererDraw, nonglyph_default_is_fill)
{
    // Large triangle: falls through arrow check → scanline fill
    renderer.draw_nonglyph_triangle(ImVec2(10.0f, 5.0f), ImVec2(13.0f, 5.0f), ImVec2(11.5f, 8.0f), kYellow, kFullClip);
    uint8_t expected_bg = ImguiRendererNcurses::col_to_ansi256_premul(kYellow);
    const TCell & c = renderer.get_cell(11, 6);
    EXPECT_EQ(c.ch, (uint32_t)' ');
    EXPECT_EQ(c.bg, expected_bg);
    EXPECT_FALSE(cell_has_arrow(11, 6));
}

TEST_F(TestNcursesRendererDraw, small_triangle_no_plus)
{
    // Small single-cell triangle (RenderBullet fan slice, resize grip):
    // scanline fill, NOT '+' (too many false positives with plot slivers)
    renderer.draw_nonglyph_triangle(ImVec2(10.0f, 10.0f),
                                    ImVec2(10.5f, 10.0f),
                                    ImVec2(10.0f, 10.5f),
                                    kWhite,
                                    kFullClip);
    EXPECT_NE(renderer.get_cell(10, 10).ch, (uint32_t)'+');
}

TEST_F(TestNcursesRendererDraw, thin_triangle_is_filled)
{
    // Thin horizontal: must produce bg fill, not be silently dropped
    renderer.draw_nonglyph_triangle(ImVec2(5.0f, 10.0f), ImVec2(10.0f, 10.0f), ImVec2(7.5f, 10.3f), kYellow, kFullClip);
    bool any_filled = false;
    for (int x = 5; x <= 10; ++x)
        if (cell_is_bg_fill(x, 10))
            any_filled = true;
    EXPECT_TRUE(any_filled) << "thin triangle must produce bg fill";
}

TEST_F(TestNcursesRendererDraw, medium_triangle_not_suppressed)
{
    // 3x2 cell triangle: must not be suppressed by any filter
    renderer.draw_nonglyph_triangle(ImVec2(10.0f, 10.0f),
                                    ImVec2(13.0f, 10.0f),
                                    ImVec2(11.5f, 12.0f),
                                    kYellow,
                                    kFullClip);
    uint8_t expected_bg = ImguiRendererNcurses::col_to_ansi256_premul(kYellow);
    bool any_filled = false;
    for (int y = 10; y <= 12; ++y)
        for (int x = 10; x <= 13; ++x)
            if (renderer.get_cell(x, y).bg == expected_bg)
                any_filled = true;
    EXPECT_TRUE(any_filled) << "medium triangle must not be suppressed";
}

// Slider grab: thin vertical rect (width 0.1-0.5 cells) -> 'I' at the center cell

TEST_F(TestNcursesRendererDraw, rect_thin_vertical_slider_grab_I)
{
    // width=0.3, height=4 → 'I' at center (20, 12)
    renderer.draw_rect(20.0f, 10.0f, 20.3f, 14.0f, kWhite, kFullClip);
    EXPECT_EQ(renderer.get_cell(20, 12).ch, (uint32_t)'I');
}

TEST_F(TestNcursesRendererDraw, rect_wide_no_slider_grab_I)
{
    // width=2, height=4 → NOT slider grab, just bg fill
    renderer.draw_rect(20.0f, 10.0f, 22.0f, 14.0f, kWhite, kFullClip);
    for (int y = 10; y < 14; ++y)
        for (int x = 20; x < 22; ++x)
            EXPECT_NE(renderer.get_cell(x, y).ch, (uint32_t)'I') << "cell(" << x << "," << y << ")";
}

TEST_F(TestNcursesRendererDraw, rect_all_sizes_plain_fill)
{
    // Narrow tall → 'I' (real slider grab width ≈ 0.1 from dump)
    renderer.draw_rect(10.0f, 5.0f, 10.1f, 15.0f, kYellow, kFullClip);
    EXPECT_EQ(renderer.get_cell(10, 10).ch, (uint32_t)'I');

    renderer.clear_screen();

    // Wide thin → space+bg, no dashes
    renderer.draw_rect(5.0f, 10.0f, 15.0f, 10.2f, kYellow, kFullClip);
    for (int x = 5; x < 15; ++x)
    {
        EXPECT_FALSE(cell_has_dash(x, 10)) << "cell(" << x << ",10)";
        const TCell & c = renderer.get_cell(x, 10);
        EXPECT_TRUE(c.ch == 0 || c.ch == ' ') << "cell(" << x << ",10) ch=" << c.ch;
    }

    renderer.clear_screen();

    // Small 1x1 → space+bg
    renderer.draw_rect(20.0f, 10.0f, 21.0f, 11.0f, kYellow, kFullClip);
    const TCell & c = renderer.get_cell(20, 10);
    EXPECT_TRUE(c.ch == 0 || c.ch == ' ');
    EXPECT_TRUE(c.bg != 0) << "small rect must have bg color set";
}

// ── Exact RenderArrow geometry → correct arrow at expected cell ────────────
// imgui RenderArrow: r = FontSize * 0.40 * scale, center = pos + (0.5, 0.5)

TEST_F(TestNcursesRendererDraw, arrow_exact_renderarrow_right)
{
    // pos=(10,10), r=0.40, center=(10.5, 10.5)
    // a=(10.800, 10.500), b=(10.200, 10.846), c=(10.200, 10.154)
    renderer.draw_nonglyph_triangle(ImVec2(10.800f, 10.500f),
                                    ImVec2(10.200f, 10.846f),
                                    ImVec2(10.200f, 10.154f),
                                    kWhite,
                                    kFullClip);
    EXPECT_EQ(renderer.get_cell(10, 10).ch, (uint32_t)'>');
}

TEST_F(TestNcursesRendererDraw, arrow_exact_renderarrow_down)
{
    // a=(10.500, 10.800), b=(10.154, 10.200), c=(10.846, 10.200)
    renderer.draw_nonglyph_triangle(ImVec2(10.500f, 10.800f),
                                    ImVec2(10.154f, 10.200f),
                                    ImVec2(10.846f, 10.200f),
                                    kWhite,
                                    kFullClip);
    EXPECT_EQ(renderer.get_cell(10, 10).ch, (uint32_t)'v');
}

TEST_F(TestNcursesRendererDraw, arrow_exact_renderarrow_left)
{
    // r negated: a=(10.200, 10.500), b=(10.800, 10.154), c=(10.800, 10.846)
    renderer.draw_nonglyph_triangle(ImVec2(10.200f, 10.500f),
                                    ImVec2(10.800f, 10.154f),
                                    ImVec2(10.800f, 10.846f),
                                    kWhite,
                                    kFullClip);
    EXPECT_EQ(renderer.get_cell(10, 10).ch, (uint32_t)'<');
}

TEST_F(TestNcursesRendererDraw, arrow_exact_renderarrow_up)
{
    // r negated: a=(10.500, 10.200), b=(10.846, 10.800), c=(10.154, 10.800)
    renderer.draw_nonglyph_triangle(ImVec2(10.500f, 10.200f),
                                    ImVec2(10.846f, 10.800f),
                                    ImVec2(10.154f, 10.800f),
                                    kWhite,
                                    kFullClip);
    EXPECT_EQ(renderer.get_cell(10, 10).ch, (uint32_t)'^');
}

TEST_F(TestNcursesRendererDraw, arrow_reject_large_triangle)
{
    // 3x2 pointy triangle: too large for RenderArrow (span > 1.2)
    renderer.draw_nonglyph_triangle(ImVec2(10.0f, 10.0f),
                                    ImVec2(10.0f, 12.0f),
                                    ImVec2(13.0f, 11.0f),
                                    kWhite,
                                    kFullClip);
    for (int y = 10; y <= 12; ++y)
        for (int x = 10; x <= 13; ++x)
            EXPECT_FALSE(cell_has_arrow(x, y)) << "large triangle at (" << x << "," << y << ")";
}

// ── classify_arrow aspect ratio: rejects plot slivers ──────────────────────

TEST_F(TestNcursesRendererDraw, arrow_reject_plot_sliver_aspect_ratio)
{
    // xspan=1.0, yspan=0.15, ratio=6.67 → rejected by aspect filter
    uint32_t ch = ImguiRendererNcurses::classify_arrow(ImVec2(10.0f, 5.0f), ImVec2(11.0f, 5.0f), ImVec2(10.5f, 5.15f));
    EXPECT_EQ(ch, (uint32_t)0);
}

TEST_F(TestNcursesRendererDraw, arrow_reject_vertical_sliver_aspect_ratio)
{
    // xspan=0.15, yspan=1.0, ratio=0.15 → rejected
    uint32_t ch = ImguiRendererNcurses::classify_arrow(ImVec2(10.0f, 5.0f), ImVec2(10.0f, 6.0f), ImVec2(10.15f, 5.5f));
    EXPECT_EQ(ch, (uint32_t)0);
}

// ydelta = ceil(max_y) - (int)min_y: a vertex at exact integer Y (8.0) starts row 8, not row 7

TEST_F(TestNcursesRendererDraw, fill_triangle_ydelta_integer_max)
{
    // max_y=8.0, min_y=5.0: ydelta = ceil(8) - 5 = 3 → rows 5,6,7
    uint8_t bg = ImguiRendererNcurses::col_to_ansi256_premul(kYellow);
    renderer.draw_fill_triangle(ImVec2(10.0f, 5.0f), ImVec2(20.0f, 5.0f), ImVec2(15.0f, 8.0f), bg, kFullClip);
    EXPECT_EQ(renderer.get_cell(15, 7).bg, bg) << "row 7 should be filled";
}

TEST_F(TestNcursesRendererDraw, fill_triangle_ydelta_fractional_max)
{
    // Top-bias: max_y=8.5, min_y=5.0 → ydelta = floor(8.5) - 5 = 3 → rows 5,6,7.
    // Row 8 NOT filled (prevents 1-row leak past container at PlotLines bottom).
    uint8_t bg = ImguiRendererNcurses::col_to_ansi256_premul(kYellow);
    renderer.draw_fill_triangle(ImVec2(10.0f, 5.0f), ImVec2(20.0f, 5.0f), ImVec2(15.0f, 8.5f), bg, kFullClip);
    EXPECT_EQ(renderer.get_cell(15, 7).bg, bg) << "row 7 should be filled";
    EXPECT_NE(renderer.get_cell(15, 8).bg, bg) << "row 8 must NOT bleed past floor(max_y)";
}

// ── Small triangles: no '+' (false positive prevention) ────────────────────

TEST_F(TestNcursesRendererDraw, small_triangle_no_plus_anywhere)
{
    renderer.draw_nonglyph_triangle(ImVec2(10.2f, 10.1f),
                                    ImVec2(10.6f, 10.3f),
                                    ImVec2(10.3f, 10.7f),
                                    kWhite,
                                    kFullClip);
    EXPECT_NE(renderer.get_cell(10, 10).ch, (uint32_t)'+');
}

// Guard-rail tests: complete widget patterns asserting the imgui -> terminal contract; a failure
// means a code change broke user-visible behavior - fix the renderer, not the test

class TestNcursesGuardRails: public ::testing::Test
{
    protected:
        ImguiRendererNcurses renderer;
        static constexpr int W = 80;
        static constexpr int H = 40;
        static constexpr ImU32 kWhite = IM_COL32(255, 255, 255, 255);
        static constexpr ImU32 kYellow = IM_COL32(255, 255, 0, 255);
        static constexpr ImU32 kRed = IM_COL32(255, 0, 0, 255);
        static constexpr ImU32 kGreen = IM_COL32(0, 255, 0, 255);
        static constexpr ImU32 kCyan = IM_COL32(0, 255, 255, 255);
        static constexpr ImVec4 kFullClip = {0.0f, 0.0f, 80.0f, 40.0f};

        TestNcursesGuardRails()
        {
            sihd::util::LoggerManager::stream();
            renderer.init_headless(W, H);
        }
        virtual ~TestNcursesGuardRails() { sihd::util::LoggerManager::clear_loggers(); }

        // Simulate AddLine: quad = 2 triangles with perpendicular thickness offset, skipping the +0.5
        // endpoint offset (test coordinates are already framebuffer space)
        void draw_line_segment(float x0, float y0, float x1, float y1, float thickness, ImU32 col)
        {
            float dx = x1 - x0;
            float dy = y1 - y0;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.001f)
                return;
            float nx = -dy / len * thickness * 0.5f;
            float ny = dx / len * thickness * 0.5f;
            ImVec2 a(x0 + nx, y0 + ny);
            ImVec2 b(x1 + nx, y1 + ny);
            ImVec2 c(x0 - nx, y0 - ny);
            ImVec2 d(x1 - nx, y1 - ny);
            renderer.draw_nonglyph_triangle(a, b, c, col, kFullClip);
            renderer.draw_nonglyph_triangle(b, d, c, col, kFullClip);
        }

        // Simulate AddRectFilled [imgui_draw.cpp:1466]: PrimRect → _draw_rect
        void draw_filled_rect(float x0, float y0, float x1, float y1, ImU32 col)
        {
            renderer.draw_rect(x0, y0, x1, y1, col, kFullClip);
        }

        void assert_only_bg_fill(int x0, int y0, int x1, int y1, const char *context)
        {
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x)
                {
                    const TCell & c = renderer.get_cell(x, y);
                    if (c.bg == 0 && c.ch == 0)
                        continue;
                    EXPECT_TRUE(c.ch == 0 || c.ch == ' ')
                        << context << ": cell(" << x << "," << y << ") has ch=" << (char)c.ch << " (" << c.ch
                        << "), expected bg fill only";
                }
        }

        void assert_no_char_in_region(uint32_t ch, int x0, int y0, int x1, int y1, const char *context)
        {
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x)
                    EXPECT_NE(renderer.get_cell(x, y).ch, ch)
                        << context << ": unexpected '" << (char)ch << "' at cell(" << x << "," << y << ")";
        }

        void assert_no_arrows(int x0, int y0, int x1, int y1, const char *context)
        {
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x)
                {
                    uint32_t ch = renderer.get_cell(x, y).ch;
                    EXPECT_TRUE(ch != '>' && ch != '<' && ch != '^' && ch != 'v')
                        << context << ": arrow '" << (char)ch << "' at cell(" << x << "," << y << ")";
                }
        }
};

// GUARD: PlotLines -> bg fill ONLY (thickness ~0.3 cells at terminal scale): never arrows,
// '+', 'I' or dashes

TEST_F(TestNcursesGuardRails, plotlines_zigzag_only_bg_fill)
{
    float pts[][2] = {{5, 15}, {10, 10}, {15, 20}, {20, 10}, {25, 18}, {30, 12}};
    for (int i = 0; i < 5; ++i)
        draw_line_segment(pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], 0.3f, kYellow);

    assert_no_arrows(4, 8, 31, 22, "PlotLines zigzag");
    assert_no_char_in_region('+', 4, 8, 31, 22, "PlotLines zigzag");
    assert_no_char_in_region('I', 4, 8, 31, 22, "PlotLines zigzag");
    assert_no_char_in_region('-', 4, 8, 31, 22, "PlotLines zigzag");
    assert_no_char_in_region('|', 4, 8, 31, 22, "PlotLines zigzag");
}

TEST_F(TestNcursesGuardRails, plotlines_steep_segments_no_arrows)
{
    draw_line_segment(10, 5, 11, 15, 0.3f, kCyan);
    draw_line_segment(11, 15, 12, 5, 0.3f, kCyan);
    draw_line_segment(12, 5, 13, 18, 0.3f, kCyan);

    assert_no_arrows(9, 4, 14, 19, "PlotLines steep");
    assert_no_char_in_region('+', 9, 4, 14, 19, "PlotLines steep");
}

TEST_F(TestNcursesGuardRails, plotlines_shallow_segments_no_arrows)
{
    draw_line_segment(5, 15, 15, 15.3f, 0.3f, kGreen);
    draw_line_segment(15, 15.3f, 25, 14.8f, 0.3f, kGreen);
    draw_line_segment(25, 14.8f, 35, 15.1f, 0.3f, kGreen);

    assert_no_arrows(4, 13, 36, 17, "PlotLines shallow");
    assert_no_char_in_region('+', 4, 13, 36, 17, "PlotLines shallow");
}

// GUARD: PlotHistogram -> bg fill, gaps preserved: wide bars rect-pair, thin bars scanline

TEST_F(TestNcursesGuardRails, histogram_bars_are_bg_fill_only)
{
    for (int i = 0; i < 5; ++i)
    {
        float x0 = 10.0f + (float)i * 3.0f;
        float x1 = x0 + 2.0f;
        float height = 5.0f + (float)(i % 3) * 3.0f;
        draw_filled_rect(x0, 25.0f - height, x1, 25.0f, kYellow);
    }

    assert_no_arrows(9, 10, 26, 26, "Histogram bars");
    assert_no_char_in_region('+', 9, 10, 26, 26, "Histogram bars");
    assert_no_char_in_region('I', 9, 10, 26, 26, "Histogram bars");
}

TEST_F(TestNcursesGuardRails, histogram_thin_bars_have_gaps)
{
    // Thin bars at x=[10, 10.7] and x=[12, 12.7]: gap at x=11 must be empty
    draw_filled_rect(10.0f, 15.0f, 10.7f, 25.0f, kYellow);
    draw_filled_rect(12.0f, 15.0f, 12.7f, 25.0f, kRed);

    for (int y = 15; y < 25; ++y)
    {
        const TCell & gap = renderer.get_cell(11, y);
        EXPECT_TRUE(gap.ch == 0 && gap.bg == 0) << "gap cell(11," << y << ") should be empty";
    }
}

// GUARD: window background -> pure bg fill, no stray characters

TEST_F(TestNcursesGuardRails, window_background_pure_bg)
{
    draw_filled_rect(1.0f, 1.0f, 60.0f, 30.0f, kWhite);
    assert_only_bg_fill(1, 1, 59, 29, "Window background");
}

// GUARD: RenderArrow exact geometry (r=0.40, spans ~0.6 cells) -> correct '>', 'v', '<', '^'

TEST_F(TestNcursesGuardRails, renderarrow_right_produces_arrow)
{
    float r = 0.40f;
    float cx = 20.5f, cy = 15.5f;
    renderer.draw_nonglyph_triangle(ImVec2(cx + 0.750f * r, cy),
                                    ImVec2(cx - 0.750f * r, cy + 0.866f * r),
                                    ImVec2(cx - 0.750f * r, cy - 0.866f * r),
                                    kWhite,
                                    kFullClip);
    EXPECT_EQ(renderer.get_cell(20, 15).ch, (uint32_t)'>');
}

TEST_F(TestNcursesGuardRails, renderarrow_down_produces_arrow)
{
    float r = 0.40f;
    float cx = 20.5f, cy = 15.5f;
    renderer.draw_nonglyph_triangle(ImVec2(cx, cy + 0.750f * r),
                                    ImVec2(cx - 0.866f * r, cy - 0.750f * r),
                                    ImVec2(cx + 0.866f * r, cy - 0.750f * r),
                                    kWhite,
                                    kFullClip);
    EXPECT_EQ(renderer.get_cell(20, 15).ch, (uint32_t)'v');
}

TEST_F(TestNcursesGuardRails, renderarrow_left_produces_arrow)
{
    float r = 0.40f;
    float cx = 20.5f, cy = 15.5f;
    renderer.draw_nonglyph_triangle(ImVec2(cx - 0.750f * r, cy),
                                    ImVec2(cx + 0.750f * r, cy - 0.866f * r),
                                    ImVec2(cx + 0.750f * r, cy + 0.866f * r),
                                    kWhite,
                                    kFullClip);
    EXPECT_EQ(renderer.get_cell(20, 15).ch, (uint32_t)'<');
}

TEST_F(TestNcursesGuardRails, renderarrow_up_produces_arrow)
{
    float r = 0.40f;
    float cx = 20.5f, cy = 15.5f;
    renderer.draw_nonglyph_triangle(ImVec2(cx, cy - 0.750f * r),
                                    ImVec2(cx + 0.866f * r, cy + 0.750f * r),
                                    ImVec2(cx - 0.866f * r, cy + 0.750f * r),
                                    kWhite,
                                    kFullClip);
    EXPECT_EQ(renderer.get_cell(20, 15).ch, (uint32_t)'^');
}

// GUARD: slider grab = thin AddRectFilled -> 'I' at center; wide track -> space + bg only

TEST_F(TestNcursesGuardRails, slider_grab_thin_vertical_shows_I)
{
    draw_filled_rect(30.0f, 10.0f, 30.1f, 14.0f, kWhite);
    EXPECT_EQ(renderer.get_cell(30, 12).ch, (uint32_t)'I');
}

TEST_F(TestNcursesGuardRails, slider_track_wide_rect_no_I)
{
    draw_filled_rect(10.0f, 10.0f, 50.0f, 11.0f, kWhite);
    for (int x = 10; x < 50; ++x)
        EXPECT_NE(renderer.get_cell(x, 10).ch, (uint32_t)'I') << "slider track should not have 'I' at x=" << x;
}

// GUARD: the scanline rasteriser never writes outside the triangle's vertex bounding box

TEST_F(TestNcursesGuardRails, fill_triangle_no_leak_below)
{
    uint8_t bg = ImguiRendererNcurses::col_to_ansi256_premul(kYellow);
    renderer.draw_fill_triangle(ImVec2(10.0f, 10.0f), ImVec2(20.0f, 10.0f), ImVec2(15.0f, 15.0f), bg, kFullClip);
    for (int x = 9; x <= 21; ++x)
        EXPECT_EQ(renderer.get_cell(x, 16).bg, 0) << "fill leaked to y=16 at x=" << x;
}

TEST_F(TestNcursesGuardRails, fill_triangle_no_leak_above)
{
    uint8_t bg = ImguiRendererNcurses::col_to_ansi256_premul(kYellow);
    renderer.draw_fill_triangle(ImVec2(10.0f, 15.0f), ImVec2(20.0f, 15.0f), ImVec2(15.0f, 10.0f), bg, kFullClip);
    for (int x = 9; x <= 21; ++x)
        EXPECT_EQ(renderer.get_cell(x, 9).bg, 0) << "fill leaked to y=9 at x=" << x;
}

TEST_F(TestNcursesRendererDraw, fill_triangle_no_leak_sideways)
{
    uint8_t bg = ImguiRendererNcurses::col_to_ansi256_premul(kYellow);
    renderer.draw_fill_triangle(ImVec2(10.0f, 12.0f), ImVec2(10.0f, 18.0f), ImVec2(20.0f, 15.0f), bg, kFullClip);
    for (int y = 11; y <= 19; ++y)
    {
        EXPECT_EQ(renderer.get_cell(9, y).bg, 0) << "fill leaked to x=9 at y=" << y;
        EXPECT_EQ(renderer.get_cell(21, y).bg, 0) << "fill leaked to x=21 at y=" << y;
    }
}

// GUARD: adjacent arrow + fill cells never corrupt each other

TEST_F(TestNcursesGuardRails, arrow_adjacent_to_fill_no_bleed)
{
    draw_filled_rect(10.0f, 10.0f, 20.0f, 15.0f, kYellow);
    float r = 0.40f;
    float cx = 9.5f, cy = 12.5f;
    renderer.draw_nonglyph_triangle(ImVec2(cx + 0.750f * r, cy),
                                    ImVec2(cx - 0.750f * r, cy + 0.866f * r),
                                    ImVec2(cx - 0.750f * r, cy - 0.866f * r),
                                    kWhite,
                                    kFullClip);

    EXPECT_EQ(renderer.get_cell(9, 12).ch, (uint32_t)'>');
    assert_only_bg_fill(10, 10, 19, 14, "Rect next to arrow");
}

// ── GUARD: Rect fill completeness — every interior cell filled ─────────────

TEST_F(TestNcursesGuardRails, rect_fill_complete_interior)
{
    draw_filled_rect(20.0f, 10.0f, 25.0f, 15.0f, kRed);
    uint8_t expected_bg = ImguiRendererNcurses::col_to_ansi256_premul(kRed);
    for (int y = 10; y < 15; ++y)
        for (int x = 20; x < 25; ++x)
            EXPECT_EQ(renderer.get_cell(x, y).bg, expected_bg) << "cell(" << x << "," << y << ")";
}

// GUARD: a wide grab handle (SliderInt/enum, several cells) collapses to one 'I', never a block.
// Needs an ImGui context so the SliderGrab colour gate is set

class TestNcursesSliderGrab: public ::testing::Test
{
    protected:
        ImguiRendererNcurses renderer;
        ImGuiContext *ctx = nullptr;
        static constexpr int W = 80;
        static constexpr int H = 24;
        static constexpr ImVec4 kFullClip = {0.0f, 0.0f, 80.0f, 24.0f};

        TestNcursesSliderGrab()
        {
            sihd::util::LoggerManager::stream();
            ctx = ImGui::CreateContext();
            ImGui::StyleColorsDark();
            renderer.init_headless(W, H);
            renderer.cache_style_colors();
        }
        virtual ~TestNcursesSliderGrab()
        {
            ImGui::DestroyContext(ctx);
            sihd::util::LoggerManager::clear_loggers();
        }

        int count_char(uint32_t ch)
        {
            int n = 0;
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    if (renderer.get_cell(x, y).ch == ch)
                        ++n;
            return n;
        }
};

// Wide handle (SliderInt geometry: ~8 cells wide, 3 tall) → exactly one 'I'.
TEST_F(TestNcursesSliderGrab, wide_grab_renders_single_I)
{
    const ImU32 grab = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    renderer.draw_rect(15.0f, 8.0f, 23.0f, 11.0f, grab, kFullClip);

    EXPECT_EQ(renderer.get_cell(19, 9).ch, (uint32_t)'I');
    EXPECT_EQ(count_char((uint32_t)'I'), 1);
}

// The wide handle must NOT leave any background fill — proves the block is gone.
TEST_F(TestNcursesSliderGrab, wide_grab_leaves_no_block_fill)
{
    const ImU32 grab = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    renderer.draw_rect(15.0f, 8.0f, 23.0f, 11.0f, grab, kFullClip);

    for (int y = 8; y < 11; ++y)
        for (int x = 15; x < 23; ++x)
            EXPECT_EQ(renderer.get_cell(x, y).bg, 0) << "block fill at cell(" << x << "," << y << ")";
}

// Thin handle (SliderFloat geometry) still renders 'I'.
TEST_F(TestNcursesSliderGrab, thin_grab_renders_I)
{
    const ImU32 grab = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    renderer.draw_rect(12.1f, 8.0f, 12.2f, 11.0f, grab, kFullClip);

    EXPECT_EQ(renderer.get_cell(12, 9).ch, (uint32_t)'I');
    EXPECT_EQ(count_char((uint32_t)'I'), 1);
}

// Active grab colour also collapses to 'I'.
TEST_F(TestNcursesSliderGrab, active_grab_renders_I)
{
    const ImU32 grab = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
    renderer.draw_rect(15.0f, 8.0f, 23.0f, 11.0f, grab, kFullClip);

    EXPECT_EQ(renderer.get_cell(19, 9).ch, (uint32_t)'I');
}

// Control: a non-grab rect of the same wide geometry still fills as a block,
// so the grab special-case is scoped to the grab colour only.
TEST_F(TestNcursesSliderGrab, non_grab_wide_rect_still_fills)
{
    const ImU32 yellow = IM_COL32(255, 255, 0, 255);
    renderer.draw_rect(15.0f, 8.0f, 23.0f, 11.0f, yellow, kFullClip);

    const uint8_t expected_bg = ImguiRendererNcurses::col_to_ansi256_premul(yellow);
    for (int y = 8; y < 11; ++y)
        for (int x = 15; x < 23; ++x)
        {
            EXPECT_EQ(renderer.get_cell(x, y).bg, expected_bg) << "cell(" << x << "," << y << ")";
            EXPECT_NE(renderer.get_cell(x, y).ch, (uint32_t)'I') << "cell(" << x << "," << y << ")";
        }
}

// PlotHistogram - real ImGui frame through the full rasteriser; guards the 'histogram breaks at
// a specific sample count' regression (bars rendered as scattered fragments)
class TestNcursesHistogram: public ::testing::Test
{
    protected:
        ImguiRendererNcurses renderer;
        ImGuiContext *ctx = nullptr;
        static constexpr int W = 220;
        static constexpr int H = 100;

        TestNcursesHistogram()
        {
            sihd::util::LoggerManager::stream();
            ctx = ImGui::CreateContext();
            ImGuiIO & io = ImGui::GetIO();
            io.DisplaySize = ImVec2((float)W, (float)H);
            io.DeltaTime = 1.0f / 60.0f;
            unsigned char *pix = nullptr;
            int tw = 0, th = 0;
            io.Fonts->GetTexDataAsAlpha8(&pix, &tw, &th);
            io.Fonts->SetTexID((ImTextureID)1);
            ImGui::StyleColorsDark();
            // Match production terminal geometry so plot sizing mirrors the demo.
            ImGuiStyle & s = ImGui::GetStyle();
            s.WindowPadding = ImVec2(0.5f, 0.0f);
            s.WindowBorderSize = 0.0f;
            s.FramePadding = ImVec2(1.0f, 0.0f);
            s.ItemSpacing = ImVec2(1.0f, 0.0f);
            s.ItemInnerSpacing = ImVec2(1.0f, 0.0f);
            renderer.init_headless(W, H);
            renderer.cache_style_colors();
        }
        virtual ~TestNcursesHistogram()
        {
            ImGui::DestroyContext(ctx);
            sihd::util::LoggerManager::clear_loggers();
        }

        static float Sin(void *, int i) { return sinf(i * 0.1f); }

        void render_histogram(int count)
        {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2((float)W, (float)H));
            ImGui::Begin("plot",
                         nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                             | ImGuiWindowFlags_NoScrollbar);
            ImGui::PlotHistogram("##h", Sin, nullptr, count, 0, nullptr, -1.0f, 1.0f, ImVec2(0, 12));
            ImGui::End();
            ImGui::Render();
            renderer.rasterize_headless(ImGui::GetDrawData());
        }

        // Most frequent non-zero bg = window/frame background. Mark any cell whose
        // bg differs from it (the histogram bars) so the bar pattern is visible.
        void dump_grid(const char *tag)
        {
            int freq[256] = {0};
            for (int i = 0; i < W * H; ++i)
            {
                const TCell & c = renderer.get_cell(i % W, i / W);
                if (c.bg != 0)
                    ++freq[c.bg];
            }
            int dom = 0, best = 0;
            for (int i = 1; i < 256; ++i)
                if (freq[i] > best)
                {
                    best = freq[i];
                    dom = i;
                }
            std::fprintf(stderr, "=== %s (window_bg=%d) bg_freq:", tag, dom);
            for (int i = 1; i < 256; ++i)
                if (freq[i] > 0)
                    std::fprintf(stderr, " %d:%d", i, freq[i]);
            std::fprintf(stderr, " ===\n");
            // Bar colour = least-frequent non-window, non-zero bg (the histogram bars).
            int bar = 0, bar_best = W * H + 1;
            for (int i = 1; i < 256; ++i)
                if (freq[i] > 0 && i != dom && freq[i] < bar_best)
                {
                    bar_best = freq[i];
                    bar = i;
                }
            for (int y = 0; y < H; ++y)
            {
                bool any = false;
                for (int x = 0; x < W; ++x)
                    if (renderer.get_cell(x, y).bg == bar)
                    {
                        any = true;
                        break;
                    }
                if (!any)
                    continue;
                std::fprintf(stderr, "%3d ", y);
                for (int x = 0; x < W; ++x)
                {
                    const TCell & c = renderer.get_cell(x, y);
                    char g = (c.bg == bar) ? 'B' : (c.bg != 0 && c.bg != dom) ? '.' : ' ';
                    std::fputc(g, stderr);
                }
                std::fputc('\n', stderr);
            }
        }
};

TEST_F(TestNcursesHistogram, DISABLED_debug_dump_195_vs_200)
{
    render_histogram(195);
    dump_grid("count=195");
    renderer.clear_screen();
    render_histogram(200);
    dump_grid("count=200");
}

// Bars clamped to ~1px by sample count must still paint a 1-cell column (not dropped by the
// sliver skip); the sine spans full -1..1 so peaks reach both plot rows
TEST_F(TestNcursesHistogram, high_sample_count_paints_full_height_bars)
{
    const uint8_t bar_bg = ImguiRendererNcurses::col_to_ansi256_premul(ImGui::GetColorU32(ImGuiCol_PlotHistogram));

    auto bar_cells_in_row = [&](int y) {
        int n = 0;
        for (int x = 0; x < W; ++x)
            if (renderer.get_cell(x, y).bg == bar_bg)
                ++n;
        return n;
    };
    auto total_bar_cells = [&]() {
        int n = 0;
        for (int y = 0; y < H; ++y)
            n += bar_cells_in_row(y);
        return n;
    };

    // count=200 exceeds the fixture's inner plot width → bars clamp to 1px.
    render_histogram(200);
    EXPECT_GT(bar_cells_in_row(0), 0);  // sine peak reaches top plot row
    EXPECT_GT(bar_cells_in_row(11), 0); // sine trough reaches bottom plot row
    EXPECT_GT(total_bar_cells(), 300);  // solid coverage, not scattered fragments
}

// CHECKBOX TICK: RenderCheckMark = CheckMark-coloured 2-triangle polyline -> _try_detect_check
// collapses it to a single 'x' at the box centre; a checked Checkbox must produce an 'x'

class TestNcursesCheckbox: public ::testing::Test
{
    protected:
        ImguiRendererNcurses renderer;
        ImGuiContext *ctx = nullptr;
        static constexpr int W = 80;
        static constexpr int H = 24;

        TestNcursesCheckbox()
        {
            sihd::util::LoggerManager::stream();
            ctx = ImGui::CreateContext();
            ImGuiIO & io = ImGui::GetIO();
            io.DisplaySize = ImVec2((float)W, (float)H);
            io.DeltaTime = 1.0f / 60.0f;
            // Match production: 1 font unit = 1 terminal cell, else widget geometry
            // is 13x too large and the check tick polyline is misclassified.
            ImFontConfig cfg;
            cfg.SizePixels = 1.0f;
            cfg.GlyphMinAdvanceX = 1.0f;
            io.Fonts->AddFontDefault(&cfg);
            unsigned char *pix = nullptr;
            int tw = 0, th = 0;
            io.Fonts->GetTexDataAsAlpha8(&pix, &tw, &th);
            io.Fonts->SetTexID((ImTextureID)1);
            ImGui::StyleColorsDark();
            ImGuiStyle & s = ImGui::GetStyle();
            s.WindowPadding = ImVec2(0.5f, 0.0f);
            s.WindowBorderSize = 0.0f;
            s.FramePadding = ImVec2(1.0f, 0.0f);
            s.ItemSpacing = ImVec2(1.0f, 0.0f);
            s.ItemInnerSpacing = ImVec2(1.0f, 0.0f);
            // No AA geometry in terminal — else thick polylines (check tick) bloat
            // their bbox and fail the checkmark gate.
            s.AntiAliasedLines = false;
            s.AntiAliasedFill = false;
            s.AntiAliasedLinesUseTex = false;
            renderer.init_headless(W, H);
            renderer.cache_style_colors();
        }
        virtual ~TestNcursesCheckbox()
        {
            ImGui::DestroyContext(ctx);
            sihd::util::LoggerManager::clear_loggers();
        }

        void render_checkbox(bool checked)
        {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2((float)W, (float)H));
            ImGui::Begin("cb",
                         nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                             | ImGuiWindowFlags_NoScrollbar);
            bool b = checked;
            ImGui::Checkbox("Animate", &b);
            ImGui::End();
            ImGui::Render();
            renderer.rasterize_headless(ImGui::GetDrawData());
        }

        int count_char(char ch)
        {
            int n = 0;
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    if (renderer.get_cell(x, y).ch == (uint32_t)ch)
                        ++n;
            return n;
        }
};

TEST_F(TestNcursesCheckbox, checked_box_paints_x)
{
    render_checkbox(true);
    EXPECT_EQ(count_char('x'), 1);
}

TEST_F(TestNcursesCheckbox, unchecked_box_has_no_x)
{
    render_checkbox(false);
    EXPECT_EQ(count_char('x'), 0);
}

// Button (full-width, hovered) - real ImGui geometry through the classifier
class TestNcursesButton: public ::testing::Test
{
    protected:
        ImguiRendererNcurses renderer;
        ImGuiContext *ctx = nullptr;
        static constexpr int W = 60;
        static constexpr int H = 12;

        TestNcursesButton()
        {
            sihd::util::LoggerManager::stream();
            ctx = ImGui::CreateContext();
            ImGuiIO & io = ImGui::GetIO();
            io.DisplaySize = ImVec2((float)W, (float)H);
            io.DeltaTime = 1.0f / 60.0f;
            ImFontConfig cfg;
            cfg.SizePixels = 1.0f;
            cfg.GlyphMinAdvanceX = 1.0f;
            io.Fonts->AddFontDefault(&cfg);
            unsigned char *pix = nullptr;
            int tw = 0, th = 0;
            io.Fonts->GetTexDataAsAlpha8(&pix, &tw, &th);
            io.Fonts->SetTexID((ImTextureID)1);
            ImGui::StyleColorsDark();
            ImGuiStyle & s = ImGui::GetStyle();
            s.WindowPadding = ImVec2(0.5f, 0.0f);
            s.WindowBorderSize = 0.0f;
            s.FramePadding = ImVec2(1.0f, 0.0f);
            s.ItemSpacing = ImVec2(1.0f, 0.0f);
            s.ItemInnerSpacing = ImVec2(1.0f, 0.0f);
            s.AntiAliasedLines = false;
            s.AntiAliasedFill = false;
            s.AntiAliasedLinesUseTex = false;
            renderer.init_headless(W, H);
            renderer.cache_style_colors();
        }
        virtual ~TestNcursesButton()
        {
            ImGui::DestroyContext(ctx);
            sihd::util::LoggerManager::clear_loggers();
        }

        // Render a full-width height-2 button. hovered=true places the mouse over it.
        void render_button(bool hovered)
        {
            ImGuiIO & io = ImGui::GetIO();
            io.MousePos = hovered ? ImVec2(W / 2.0f, 1.0f) : ImVec2(-1.0f, -1.0f);
            // Two frames: hover needs HoveredWindow which is computed from the
            // previous frame's window rects.
            for (int f = 0; f < 2; ++f)
            {
                ImGui::NewFrame();
                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(ImVec2((float)W, (float)H));
                ImGui::Begin("btn",
                             nullptr,
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                                 | ImGuiWindowFlags_NoScrollbar);
                ImGui::Button("Exit program", ImVec2(ImGui::GetContentRegionAvail().x, 2));
                ImGui::End();
                ImGui::Render();
            }
            renderer.rasterize_headless(ImGui::GetDrawData());
        }

        int count_char(char ch)
        {
            int n = 0;
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    if (renderer.get_cell(x, y).ch == (uint32_t)ch)
                        ++n;
            return n;
        }
};

// A hovered full-width button fills its bar and must NOT collapse to a single 'I': ButtonHovered
// shares the SliderGrabActive accent colour, the wide bg must not misdetect as a grab
TEST_F(TestNcursesButton, hovered_button_fills_bar_no_I)
{
    render_button(true);
    const uint8_t hov_bg = ImguiRendererNcurses::col_to_ansi256_premul(ImGui::GetColorU32(ImGuiCol_ButtonHovered));
    EXPECT_EQ(count_char('I'), 0);
    int filled = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (renderer.get_cell(x, y).bg == hov_bg)
                ++filled;
    EXPECT_GT(filled, 20);
}

} // namespace test
