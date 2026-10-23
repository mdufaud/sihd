#include <imgui.h>

#include <array>

#include <gtest/gtest.h>

#include "helper/NcursesScene.hpp"

namespace test
{

class TestNcursesDraw: public NcursesScene
{
    protected:
        TestNcursesDraw(): NcursesScene(24, 8) {}
};

TEST_F(TestNcursesDraw, image_is_drawn_as_a_shaded_block)
{
    render_scene([] { ImGui::Image(ImTextureRef((ImTextureID)7), ImVec2(4, 2)); });

    EXPECT_EQ(row_text(1), "│▒▒▒▒                  │");
    EXPECT_EQ(row_text(2), "│▒▒▒▒                  │");
}

TEST_F(TestNcursesDraw, font_atlas_image_is_drawn_as_a_shaded_block)
{
    render_scene([] { ImGui::Image(ImGui::GetIO().Fonts->TexRef, ImVec2(4, 2)); });

    EXPECT_EQ(row_text(1), "│▒▒▒▒                  │");
    EXPECT_EQ(row_text(2), "│▒▒▒▒                  │");
}

TEST_F(TestNcursesDraw, huge_rect_paints_only_its_visible_cells)
{
    render_scene([] {
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(-100000, -100000),
                                                  ImVec2(100000, 100000),
                                                  IM_COL32(255, 0, 0, 255));
    });

    EXPECT_EQ(row_backgrounds(0), "aaaaaaaaaaaaaaaaaaaaaaaa");
    EXPECT_EQ(row_backgrounds(3), "abbbbbbbbbbbbbbbbbbbbbba");
    EXPECT_EQ(row_backgrounds(7), "aaaaaaaaaaaaaaaaaaaaaaaa");
}

TEST_F(TestNcursesDraw, multicolor_rect_keeps_its_gradient)
{
    render_scene([] {
        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(ImVec2(2, 2),
                                                            ImVec2(10, 3),
                                                            IM_COL32(0, 0, 0, 255),
                                                            IM_COL32(255, 0, 0, 255),
                                                            IM_COL32(255, 0, 0, 255),
                                                            IM_COL32(0, 0, 0, 255));
    });

    EXPECT_EQ(row_backgrounds(2), "aabcdefghiaaaaaaaaaaaaaa");
    EXPECT_EQ(cell(2, 2).bg, 16u);
    EXPECT_EQ(cell(9, 2).bg, 239u);
}

TEST_F(TestNcursesDraw, white_user_rect_is_a_fill)
{
    render_scene(
        [] { ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(2, 2), ImVec2(10, 5), IM_COL32(255, 255, 255, 255)); });

    EXPECT_EQ(screen_text(),
              "┌──────────────────────┐\n"
              "│                      │\n"
              "│                      │\n"
              "│                      │\n"
              "│                      │\n"
              "│                      │\n"
              "│                      │\n"
              "└──────────────────────┘");
    EXPECT_EQ(row_backgrounds(3), "aabbbbbbbbaaaaaaaaaaaaaa");
}

TEST_F(TestNcursesDraw, white_user_triangle_is_a_fill)
{
    render_scene([] {
        ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(2, 2),
                                                      ImVec2(10, 2),
                                                      ImVec2(6, 6),
                                                      IM_COL32(255, 255, 255, 255));
    });

    EXPECT_EQ(screen_text(),
              "┌──────────────────────┐\n"
              "│                      │\n"
              "│                      │\n"
              "│                      │\n"
              "│                      │\n"
              "│                      │\n"
              "│                      │\n"
              "└──────────────────────┘");
    EXPECT_EQ(row_backgrounds(2), "aabbbbbbbbaaaaaaaaaaaaaa");
}

TEST_F(TestNcursesDraw, histogram_with_more_samples_than_cells_fills_full_bars)
{
    std::array<float, 40> samples {};
    samples.fill(1.0f);

    render_scene([&] {
        ImGui::PlotHistogram("##h", samples.data(), (int)samples.size(), 0, nullptr, 0.0f, 1.0f, ImVec2(20, 3));
    });

    EXPECT_EQ(row_backgrounds(1), "abccccccccccccccccccbaaa");
    EXPECT_EQ(row_backgrounds(3), "abccccccccccccccccccbaaa");
}

TEST_F(TestNcursesDraw, histogram_bar_tops_use_eighth_blocks)
{
    const std::array<float, 4> samples {0.3f, 1.0f, 0.6f, 0.9f};

    render_scene([&] {
        ImGui::PlotHistogram("##h", samples.data(), (int)samples.size(), 0, nullptr, 0.0f, 1.0f, ImVec2(8, 2));
    });

    EXPECT_EQ(row_text(1), "│    ▂▂▆               │");
    EXPECT_EQ(row_text(2), "│ ▅▅                   │");
}

TEST_F(TestNcursesDraw, radio_button_shows_only_its_label)
{
    int choice = 0;

    render_scene([&] {
        ImGui::RadioButton("a", &choice, 0);
        ImGui::RadioButton("b", &choice, 1);
    });

    EXPECT_EQ(row_text(1), "│  a                   │");
    EXPECT_EQ(row_text(2), "│  b                   │");
}

TEST_F(TestNcursesDraw, larger_font_size_keeps_one_cell_per_character_but_stretches_rows)
{
    render_scene([] {
        ImGui::PushFont(nullptr, 2.0f);
        ImGui::TextUnformatted("big");
        ImGui::PopFont();
        ImGui::TextUnformatted("next");
    });

    EXPECT_EQ(screen_text(),
              "┌──────────────────────┐\n"
              "│big                   │\n"
              "│                      │\n"
              "│next                  │\n"
              "│                      │\n"
              "│                      │\n"
              "│                      │\n"
              "└──────────────────────┘");
}

} // namespace test
