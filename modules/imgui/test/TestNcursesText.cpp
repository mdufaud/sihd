#include <imgui.h>

#include <gtest/gtest.h>

#include "helper/NcursesScene.hpp"

namespace test
{

class TestNcursesText: public NcursesScene
{
    protected:
        TestNcursesText(): NcursesScene(24, 5) {}
};

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
