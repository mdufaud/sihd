#include <imgui.h>
#include <imgui_internal.h>

#include <gtest/gtest.h>

#include "helper/NcursesScene.hpp"

namespace test
{

class TestNcursesShapes: public NcursesScene
{
    protected:
        TestNcursesShapes(): NcursesScene(24, 6) {}
};

TEST_F(TestNcursesShapes, gradient_starting_on_a_style_colour_stays_a_fill)
{
    render_scene([] {
        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(ImVec2(3, 1),
                                                            ImVec2(4, 4),
                                                            IM_COL32(255, 255, 0, 255),
                                                            IM_COL32(255, 255, 0, 255),
                                                            IM_COL32(0, 255, 0, 255),
                                                            IM_COL32(0, 255, 0, 255));
    });

    EXPECT_EQ(row_text(2), "│                      │");
    EXPECT_EQ(row_backgrounds(1), "aaabaaaaaaaaaaaaaaaaaaaa");
}

TEST_F(TestNcursesShapes, table_without_outer_borders_keeps_its_column_border_inside)
{
    render_scene([] {
        ImGui::TextUnformatted("above");
        if (ImGui::BeginTable("table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("a");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted("b");
            ImGui::EndTable();
        }
    });

    EXPECT_EQ(row_text(1), "│above                 │");
    EXPECT_EQ(row_text(2), "│a         │ b         │");
}

TEST_F(TestNcursesShapes, tree_line_reaches_each_child_on_its_own_row)
{
    render_scene([] {
        const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_DrawLinesFull;
        if (ImGui::TreeNodeEx("Parent", flags))
        {
            if (ImGui::TreeNodeEx("Child", flags))
            {
                ImGui::TextUnformatted("leaf");
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    });

    EXPECT_EQ(row_text(1), "│ ▼ Parent             │");
    EXPECT_EQ(row_text(2), "│ │─▼ Child            │");
    EXPECT_EQ(row_text(3), "│    leaf              │");
}

TEST_F(TestNcursesShapes, mixed_checkbox_shows_a_dash)
{
    bool value = false;

    render_scene([&] {
        ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
        ImGui::Checkbox("mixed", &value);
        ImGui::PopItemFlag();
        ImGui::Checkbox("off", &value);
    });

    EXPECT_EQ(row_text(1), "│- mixed               │");
    EXPECT_EQ(row_text(2), "│  off                 │");
}

} // namespace test
