#include <imgui.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/imgui/ImguiRendererNcurses.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::imgui;
using namespace sihd::util;

namespace
{

using CrossPoints = std::array<ImVec2, ImguiRendererNcurses::cross_pattern_vertex_count>;

struct ArrowCase
{
        std::string name;
        ImVec2 p0;
        ImVec2 p1;
        ImVec2 p2;
        uint32_t expected;
};

struct CrossCase
{
        std::string name;
        CrossPoints points;
        bool expected;
};

CrossPoints x_shape(float left, float top, float span, float half_width)
{
    const float right = left + span;
    const float bottom = top + span;
    return {{
        {left - half_width, top + half_width},
        {right - half_width, bottom + half_width},
        {left + half_width, top - half_width},
        {left + half_width, top - half_width},
        {right - half_width, bottom + half_width},
        {right + half_width, bottom - half_width},
        {right + half_width, top + half_width},
        {left + half_width, bottom + half_width},
        {right - half_width, top - half_width},
        {right - half_width, top - half_width},
        {left + half_width, bottom + half_width},
        {left - half_width, bottom - half_width},
    }};
}

void expect_arrows(const std::vector<ArrowCase> & cases)
{
    for (const ArrowCase & arrow : cases)
    {
        SCOPED_TRACE(arrow.name);
        EXPECT_EQ(ImguiRendererNcurses::classify_arrow(arrow.p0, arrow.p1, arrow.p2), arrow.expected);
    }
}

void expect_crosses(const std::vector<CrossCase> & cases)
{
    for (const CrossCase & cross : cases)
    {
        SCOPED_TRACE(cross.name);
        EXPECT_EQ(ImguiRendererNcurses::is_cross_pattern(cross.points.data()), cross.expected);
    }
}

} // namespace

class TestNcursesRenderer: public ::testing::Test
{
    protected:
        TestNcursesRenderer() { LoggerManager::stream(); }
        ~TestNcursesRenderer() override { LoggerManager::clear_loggers(); }
};

TEST_F(TestNcursesRenderer, rgb_to_ansi256_maps_colours_to_the_cube)
{
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(255, 0, 0), 196);
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(0, 255, 0), 46);
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(0, 0, 255), 21);
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(255, 255, 0), 226);
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(0, 255, 255), 51);
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(255, 0, 255), 201);
}

TEST_F(TestNcursesRenderer, rgb_to_ansi256_maps_grays_to_the_ramp)
{
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(0, 0, 0), 16);
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(1, 1, 1), 16);
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(50, 50, 50), 236);
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(128, 128, 128), 244);
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(200, 200, 200), 251);
    EXPECT_EQ(ImguiRendererNcurses::rgb_to_ansi256(255, 255, 255), 231);
}

TEST_F(TestNcursesRenderer, classify_arrow_reads_the_pointing_direction)
{
    expect_arrows({
        {"right", {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.5f}, '>'},
        {"left", {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.5f}, '<'},
        {"down", {0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 1.0f}, 'v'},
        {"up", {0.0f, 1.0f}, {1.0f, 1.0f}, {0.5f, 0.0f}, '^'},
        {"equilateral down", {0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 0.866f}, 'v'},
        {"right with rotated vertex order", {1.0f, 0.5f}, {0.0f, 0.0f}, {0.0f, 1.0f}, '>'},
        {"right with a wide base", {0.0f, 0.0f}, {0.0f, 1.2f}, {1.0f, 0.6f}, '>'},
        {"flat down", {0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 0.5f}, 'v'},
    });
}

TEST_F(TestNcursesRenderer, classify_arrow_reads_widget_arrows)
{
    expect_arrows({
        {"combo dropdown", {68.0f, 3.0f}, {68.0f, 4.0f}, {69.0f, 3.5f}, '>'},
        {"tree node closed", {2.0f, 5.0f}, {2.0f, 6.0f}, {3.0f, 5.5f}, '>'},
        {"tree node open", {2.0f, 5.0f}, {3.0f, 5.0f}, {2.5f, 6.0f}, 'v'},
        {"scrollbar up", {70.0f, 2.0f}, {71.0f, 2.0f}, {70.5f, 1.0f}, '^'},
        {"scrollbar down", {70.0f, 20.0f}, {71.0f, 20.0f}, {70.5f, 21.0f}, 'v'},
    });
}

TEST_F(TestNcursesRenderer, classify_arrow_rejects_other_triangles)
{
    expect_arrows({
        {"narrow base", {0.3f, 0.0f}, {0.7f, 0.0f}, {0.5f, 1.0f}, 0},
        {"very narrow base", {0.4f, 0.0f}, {0.6f, 0.0f}, {0.5f, 1.0f}, 0},
        {"right angle", {0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, 0},
        {"collinear", {0.0f, 0.0f}, {1.0f, 0.0f}, {2.0f, 0.0f}, 0},
        {"single point", {5.0f, 5.0f}, {5.0f, 5.0f}, {5.0f, 5.0f}, 0},
        {"checkbox corner", {5.0f, 3.0f}, {6.0f, 3.0f}, {5.0f, 4.0f}, 0},
        {"rect half", {10.0f, 5.0f}, {15.0f, 5.0f}, {10.0f, 8.0f}, 0},
        {"circle fan slice", {10.0f, 10.0f}, {11.0f, 10.5f}, {10.8f, 11.0f}, 0},
        {"resize grip", {79.0f, 24.0f}, {79.0f, 23.0f}, {78.0f, 24.0f}, 0},
        {"flat line quad half", {5.0f, 3.15f}, {6.0f, 3.15f}, {5.0f, 2.85f}, 0},
        {"long line quad half", {0.0f, 0.1f}, {3.0f, 0.1f}, {0.0f, -0.1f}, 0},
        {"thin bar half", {10.0f, 5.0f}, {10.3f, 5.0f}, {10.0f, 10.0f}, 0},
        {"near zero bar half", {15.0f, 16.0f}, {15.8f, 16.0f}, {15.0f, 15.9f}, 0},
    });
}

TEST_F(TestNcursesRenderer, compute_clip_rect_applies_offset_and_scale)
{
    ImDrawCmd cmd;
    cmd.ClipRect = ImVec4(10.0f, 20.0f, 100.0f, 200.0f);

    const ImVec4 identity = ImguiRendererNcurses::compute_clip_rect(cmd, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
    const ImVec4 moved = ImguiRendererNcurses::compute_clip_rect(cmd, ImVec2(5.0f, 10.0f), ImVec2(2.0f, 2.0f));
    const ImVec4 negative = ImguiRendererNcurses::compute_clip_rect(cmd, ImVec2(-10.0f, -5.0f), ImVec2(0.5f, 0.5f));

    EXPECT_FLOAT_EQ(identity.x, 10.0f);
    EXPECT_FLOAT_EQ(identity.y, 20.0f);
    EXPECT_FLOAT_EQ(identity.z, 100.0f);
    EXPECT_FLOAT_EQ(identity.w, 200.0f);
    EXPECT_FLOAT_EQ(moved.x, 10.0f);
    EXPECT_FLOAT_EQ(moved.y, 20.0f);
    EXPECT_FLOAT_EQ(moved.z, 190.0f);
    EXPECT_FLOAT_EQ(moved.w, 380.0f);
    EXPECT_FLOAT_EQ(negative.x, 10.0f);
    EXPECT_FLOAT_EQ(negative.y, 12.5f);
    EXPECT_FLOAT_EQ(negative.z, 55.0f);
    EXPECT_FLOAT_EQ(negative.w, 102.5f);
}

TEST_F(TestNcursesRenderer, is_cross_pattern_accepts_close_button_crosses)
{
    expect_crosses({
        {"close button", x_shape(4.0f, 4.0f, 2.0f, 0.15f), true},
        {"smaller", x_shape(1.0f, 1.0f, 1.0f, 0.1f), true},
        {"offset", x_shape(19.0f, 14.0f, 2.0f, 0.15f), true},
        {"smallest accepted span", x_shape(0.0f, 0.0f, 0.5f, 0.05f), true},
    });
}

TEST_F(TestNcursesRenderer, is_cross_pattern_rejects_other_line_pairs)
{
    CrossPoints same_point {};
    same_point.fill(ImVec2(5.0f, 5.0f));
    CrossPoints collinear {};
    for (size_t i = 0; i < collinear.size(); ++i)
        collinear[i] = ImVec2((float)i * 0.2f, (float)i * 0.2f);

    expect_crosses({
        {"too large", x_shape(0.0f, 0.0f, 4.0f, 0.15f), false},
        {"just past the largest span", x_shape(0.0f, 0.0f, 2.95f, 0.1f), false},
        {"too small", x_shape(5.0f, 5.0f, 0.05f, 0.05f), false},
        {"same point", same_point, false},
        {"collinear points", collinear, false},
        {"narrow horizontal",
         {{{0.0f, 5.0f},
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
           {3.0f, 4.7f}}},
         false},
        {"narrow vertical",
         {{{5.0f, 0.0f},
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
           {4.7f, 3.0f}}},
         false},
        {"sequential plot segments",
         {{{10.0f, 5.1f},
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
           {12.0f, 5.9f}}},
         false},
        {"parallel lines",
         {{{1.0f, 1.0f},
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
           {3.0f, 2.2f}}},
         false},
        {"v shape",
         {{{1.0f, 1.1f},
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
           {2.0f, 1.9f}}},
         false},
        {"l shape",
         {{{1.0f, 3.15f},
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
           {2.85f, 1.0f}}},
         false},
        {"plot zigzag",
         {{{0.0f, 1.1f},
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
           {2.0f, 0.9f}}},
         false},
        {"horizontal plot segments",
         {{{0.0f, 3.1f},
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
           {4.0f, 4.9f}}},
         false},
    });
}

} // namespace test
