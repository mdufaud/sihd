#include <imgui.h>

#include <gtest/gtest.h>

#include "helper/NcursesScene.hpp"

namespace test
{

class TestNcursesText: public NcursesScene
{
    protected:
        TestNcursesText(): NcursesScene(24, 5) {}

        void link_scene()
        {
            this->scene_window([] {
                ImGui::TextUnformatted("a:");
                ImGui::SameLine();
                ImGui::TextLink("link");
                ImGui::TextUnformatted("below");
            });
        }

        std::string underlined_columns(int y) const
        {
            std::string columns;
            for (int x = 0; x < width; ++x)
                columns += cell(x, y).underline ? 'u' : '.';
            return columns;
        }
};

TEST_F(TestNcursesText, link_is_underlined_on_its_text_row)
{
    render_frames(2, [&] { link_scene(); });

    EXPECT_EQ(row_text(1), "│a: link               │");
    EXPECT_EQ(underlined_columns(1), "....uuuu................");
    EXPECT_EQ(row_backgrounds(2), "aaaaaaaaaaaaaaaaaaaaaaaa");
}

TEST_F(TestNcursesText, hovered_link_stays_underlined_on_its_text_row)
{
    ImGuiIO & io = ImGui::GetIO();

    render_frames(2, [&] { link_scene(); });
    io.AddMousePosEvent(5.0f, 1.0f);
    render_frames(2, [&] { link_scene(); });

    EXPECT_EQ(underlined_columns(1), "....uuuu................");
    EXPECT_EQ(row_backgrounds(2), "aaaaaaaaaaaaaaaaaaaaaaaa");
    EXPECT_EQ(row_text(2), "│below                 │");
}

TEST_F(TestNcursesText, label_clipped_by_its_button_stops_at_the_button_edge)
{
    render_scene([] {
        ImGui::Button("FizzBuzz", ImVec2(3, 1));
        ImGui::SameLine();
        ImGui::TextUnformatted("x");
    });

    EXPECT_EQ(row_text(1), "│ Fi x                 │");
}

TEST_F(TestNcursesText, tab_advances_four_blank_cells)
{
    render_scene([] { ImGui::TextUnformatted("a\tb"); });

    EXPECT_EQ(row_text(1), "│a    b                │");
}

} // namespace test
