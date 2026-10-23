#include <imgui.h>

#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "helper/NcursesScene.hpp"

namespace test
{

namespace
{

float wave(void *, int index)
{
    return std::sin((float)index * 0.3f);
}

} // namespace

class TestNcursesWidgets: public NcursesScene
{
    protected:
        TestNcursesWidgets(): NcursesScene(24, 8) {}
};

TEST_F(TestNcursesWidgets, checked_checkbox_shows_a_check_mark)
{
    bool checked = true;
    bool unchecked = false;

    render_scene([&] {
        ImGui::Checkbox("on", &checked);
        ImGui::Checkbox("off", &unchecked);
    });

    EXPECT_EQ(row_text(1), "│✓ on                  │");
    EXPECT_EQ(row_text(2), "│  off                 │");
    EXPECT_NE(cell(1, 1).bg, cell(1, 2).bg);
}

TEST_F(TestNcursesWidgets, tree_nodes_show_arrows_and_bullets)
{
    render_scene([] {
        if (ImGui::TreeNode("closed"))
            ImGui::TreePop();
        ImGui::SetNextItemOpen(true);
        if (ImGui::TreeNode("open"))
        {
            ImGui::BulletText("leaf");
            ImGui::TreePop();
        }
    });

    EXPECT_EQ(row_text(1), "│ ▶ closed             │");
    EXPECT_EQ(row_text(2), "│ ▼ open               │");
    EXPECT_EQ(row_text(3), "│   • leaf             │");
}

TEST_F(TestNcursesWidgets, selected_tab_has_its_own_background)
{
    render_scene([] {
        if (ImGui::BeginTabBar("tabs"))
        {
            if (ImGui::BeginTabItem("T1"))
                ImGui::EndTabItem();
            if (ImGui::BeginTabItem("T2"))
                ImGui::EndTabItem();
            ImGui::EndTabBar();
        }
    });

    EXPECT_EQ(row_text(1), "│ T1    T2             │");
    EXPECT_EQ(row_backgrounds(1), "abbbbbacccccaaaaaaaaaaaa");
}

TEST_F(TestNcursesWidgets, selected_tab_overline_underlines_it)
{
    render_scene([] {
        if (ImGui::BeginTabBar("tabs", ImGuiTabBarFlags_DrawSelectedOverline))
        {
            if (ImGui::BeginTabItem("T1"))
                ImGui::EndTabItem();
            if (ImGui::BeginTabItem("T2"))
                ImGui::EndTabItem();
            ImGui::EndTabBar();
        }
    });

    std::string underlined;
    for (int x = 0; x < width; ++x)
        underlined += cell(x, 1).underline ? 'u' : '.';
    EXPECT_EQ(underlined, ".uuuuu..................");
}

TEST_F(TestNcursesWidgets, vertical_scrollbar_is_visible)
{
    render_scene([] {
        for (int line = 0; line < 30; ++line)
            ImGui::Text("line %d", line);
    });

    EXPECT_EQ(screen_text(),
              "┌──────────────────────┐\n"
              "│line 0               █│\n"
              "│line 1               ░│\n"
              "│line 2               ░│\n"
              "│line 3               ░│\n"
              "│line 4               ░│\n"
              "│line 5               ░│\n"
              "└──────────────────────┘");
}

TEST_F(TestNcursesWidgets, slider_grab_marks_the_value)
{
    float value = 0.5f;

    render_scene([&] { ImGui::SliderFloat("##s", &value, 0.0f, 1.0f, "%.1f", ImGuiSliderFlags_None); });

    EXPECT_EQ(row_text(1), "│      0.5             │");
    EXPECT_EQ(row_backgrounds(1), "abbbbbbbcbbbbbbbaaaaaaaa");
}

TEST_F(TestNcursesWidgets, progress_bar_ends_with_an_eighth_block)
{
    render_scene([] { ImGui::ProgressBar(0.55f, ImVec2(10, 1), ""); });

    EXPECT_EQ(row_text(1), "│     ▌                │");
    EXPECT_EQ(row_backgrounds(1), "abbbbbcccccaaaaaaaaaaaaa");
}

TEST_F(TestNcursesWidgets, plot_lines_draw_braille_dots)
{
    render_scene([] { ImGui::PlotLines("##p", wave, nullptr, 30, 0, nullptr, -1.0f, 1.0f, ImVec2(20, 4)); });

    EXPECT_EQ(screen_text(),
              "┌──────────────────────┐\n"
              "│  ⡰⠊⠉⠑⡄        ⢠⠒⠉⠙   │\n"
              "│ ⡰⠁   ⠈⢦      ⣠⠃      │\n"
              "│ ⠁      ⢇    ⢰⠁       │\n"
              "│         ⠱⣀⣀⠔⠁        │\n"
              "│                      │\n"
              "│                      │\n"
              "└──────────────────────┘");
}

TEST_F(TestNcursesWidgets, popup_menu_is_boxed_with_its_check_mark)
{
    bool enabled = true;

    render_frames(3, [&] {
        ImGui::OpenPopup("menu");
        ImGui::SetNextWindowPos(ImVec2(2, 1));
        if (ImGui::BeginPopup("menu"))
        {
            ImGui::MenuItem("enabled", nullptr, &enabled);
            ImGui::MenuItem("other");
            ImGui::EndPopup();
        }
    });

    EXPECT_EQ(screen_text(),
              "\n"
              "  ┌─────────┐\n"
              "  │enabled ✓│\n"
              "  │other    │\n"
              "  └─────────┘");
}

TEST_F(TestNcursesWidgets, modal_dims_what_is_behind_it)
{
    render_frames(3, [this] {
        scene_window([] { ImGui::TextUnformatted("behind"); });
        ImGui::OpenPopup("modal");
        ImGui::SetNextWindowPos(ImVec2(12, 4));
        if (ImGui::BeginPopupModal("modal", nullptr, ImGuiWindowFlags_NoTitleBar))
        {
            ImGui::TextUnformatted("ok");
            ImGui::EndPopup();
        }
    });

    EXPECT_TRUE(cell(1, 1).dim);
    EXPECT_FALSE(cell(13, 5).dim);
}

} // namespace test
