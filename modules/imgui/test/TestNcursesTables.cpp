#include <imgui.h>

#include <gtest/gtest.h>

#include "helper/NcursesScene.hpp"

namespace test
{

class TestNcursesTables: public NcursesScene
{
    protected:
        TestNcursesTables(): NcursesScene(24, 9) {}

        static void scrolling_table(ImGuiTableFlags flags, const char *first_column = "name", float scroll_rows = 0.0f)
        {
            if (!ImGui::BeginTable("table", 2, ImGuiTableFlags_ScrollY | flags, ImVec2(0, 6)))
                return;
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn(first_column);
            ImGui::TableSetupColumn("size");
            ImGui::TableHeadersRow();
            for (int row = 0; row < 10; ++row)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("a%d", row);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("b%d", row);
            }
            if (scroll_rows > 0.0f)
                ImGui::SetScrollY(scroll_rows);
            ImGui::EndTable();
        }
};

TEST_F(TestNcursesTables, scrolling_table_with_outer_horizontal_borders_keeps_its_header)
{
    render_scene([] { scrolling_table(ImGuiTableFlags_BordersOuter); });

    EXPECT_EQ(row_text(1), "││ name     size      █│");
    EXPECT_EQ(row_text(2), "││ a0       b0        █│");
}

TEST_F(TestNcursesTables, restored_header_has_the_header_background)
{
    render_scene([] { scrolling_table(ImGuiTableFlags_BordersOuter); });

    EXPECT_EQ(row_backgrounds(1), "aabbbbbbbbbbbbbbbbbbbbca");
    EXPECT_EQ(row_backgrounds(2), "aaaaaaaaaaaaaaaaaaaaaaba");
}

TEST_F(TestNcursesTables, restored_header_hides_the_id_suffix_of_a_label)
{
    render_scene([] { scrolling_table(ImGuiTableFlags_BordersOuter, "name##column"); });

    EXPECT_EQ(row_text(1), "││ name     size      █│");
}

TEST_F(TestNcursesTables, restored_header_stays_above_scrolled_rows)
{
    render_frames(4, [this] { scene_window([] { scrolling_table(ImGuiTableFlags_BordersOuter, "name", 3.0f); }); });

    EXPECT_EQ(row_text(1), "││ name     size      ░│");
    EXPECT_EQ(row_text(3), "││ a4       b4        █│");
}

TEST_F(TestNcursesTables, table_without_outer_horizontal_borders_is_unchanged)
{
    render_scene([] { scrolling_table(ImGuiTableFlags_BordersOuterV); });

    EXPECT_EQ(row_text(1), "││ name     size      █│");
    EXPECT_EQ(row_text(6), "││ a4       b4        ░│");
    EXPECT_EQ(row_backgrounds(1), "aabbbbbbbbbbbbbbbbbbbbca");
}

} // namespace test
