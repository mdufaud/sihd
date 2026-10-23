#include <imgui.h>

#include <array>

#include <gtest/gtest.h>

#include "helper/NcursesScene.hpp"

namespace test
{

class TestNcursesLayout: public NcursesScene
{
    protected:
        TestNcursesLayout(): NcursesScene(26, 8) {}
};

TEST_F(TestNcursesLayout, titled_window_has_a_frame_and_a_resize_grip)
{
    render_frames(2, [] {
        ImGui::SetNextWindowPos(ImVec2(1, 1));
        ImGui::SetNextWindowSize(ImVec2(20, 6));
        ImGui::Begin("My Title", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("body");
        ImGui::End();
    });

    EXPECT_EQ(screen_text(),
              "\n"
              " ┌─▼──My Title──────┐\n"
              " │                  │\n"
              " │body              │\n"
              " │                  │\n"
              " │                 ◢│\n"
              " └──────────────────┘");
}

TEST_F(TestNcursesLayout, closable_window_shows_a_close_cross)
{
    bool open = true;

    render_frames(2, [&] {
        ImGui::SetNextWindowPos(ImVec2(1, 1));
        ImGui::SetNextWindowSize(ImVec2(20, 4));
        ImGui::Begin("Closable", &open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize);
        ImGui::End();
    });

    EXPECT_EQ(screen_text(),
              "\n"
              " ┌─▼──Closable────×─┐\n"
              " │                  │\n"
              " │                  │\n"
              " └──────────────────┘");
}

TEST_F(TestNcursesLayout, table_keeps_vertical_borders_only)
{
    render_scene([] {
        if (ImGui::BeginTable("table", 2, ImGuiTableFlags_Borders))
        {
            for (int row = 0; row < 2; ++row)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("a%d", row);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("b%d", row);
            }
            ImGui::EndTable();
        }
    });

    EXPECT_EQ(screen_text(),
              "┌────────────────────────┐\n"
              "││ a0       │ b0        ││\n"
              "││ a1       │ b1        ││\n"
              "│                        │\n"
              "│                        │\n"
              "│                        │\n"
              "│                        │\n"
              "└────────────────────────┘");
}

TEST_F(TestNcursesLayout, child_window_is_boxed)
{
    render_scene([] {
        ImGui::BeginChild("child", ImVec2(12, 4), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("inside");
        ImGui::EndChild();
    });

    EXPECT_EQ(row_text(1), "│┌──────────┐            │");
    EXPECT_EQ(row_text(2), "││inside    │            │");
    EXPECT_EQ(row_text(4), "│└──────────┘            │");
}

TEST_F(TestNcursesLayout, separator_draws_a_line)
{
    render_scene([] {
        ImGui::TextUnformatted("up");
        ImGui::Separator();
        ImGui::TextUnformatted("down");
    });

    EXPECT_EQ(row_text(2), "│────────────────────────│");
}

TEST_F(TestNcursesLayout, separator_text_line_sits_on_its_label_row)
{
    render_scene([] {
        ImGui::SeparatorText("Title");
        ImGui::TextUnformatted("below");
    });

    EXPECT_EQ(row_text(1), "│ Title ─────────────────│");
    EXPECT_EQ(row_text(2), "│below                   │");
}

TEST_F(TestNcursesLayout, window_placed_off_screen_keeps_its_title_bar_visible)
{
    render_frames(3, [] {
        ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(10, 4), ImGuiCond_FirstUseEver);
        ImGui::Begin("Far", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::End();
    });

    EXPECT_EQ(row_text(7), "                      ┌─▼─");
}

TEST_F(TestNcursesLayout, translucent_fullscreen_window_keeps_its_background)
{
    render_frames(2, [this] {
        ImGui::SetNextWindowBgAlpha(0.94f);
        scene_window([] { ImGui::TextUnformatted("popup"); });
    });

    EXPECT_EQ(row_backgrounds(height / 2), "aaaaaaaaaaaaaaaaaaaaaaaaaa");
}

TEST_F(TestNcursesLayout, large_multiline_input_has_a_frame_background)
{
    std::array<char, 64> text {"multi"};

    render_scene([&] {
        ImGui::InputTextMultiline("##m", text.data(), text.size(), ImVec2((float)width - 4, (float)height - 3));
    });

    EXPECT_EQ(row_backgrounds(3), "abbbbbbbbbbbbbbbbbbbbbbaaa");
}

TEST_F(TestNcursesLayout, text_after_more_vertices_than_16_bit_indices_still_renders)
{
    constexpr int offscreen_rects = 17000;

    render_scene([] {
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        for (int i = 0; i < offscreen_rects; ++i)
            draw_list->AddRectFilled(ImVec2(-10.0f, -10.0f), ImVec2(-9.0f, -9.0f), IM_COL32_WHITE);
        ImGui::TextUnformatted("last");
    });

    EXPECT_EQ(row_text(1), "│last                    │");
}

} // namespace test
