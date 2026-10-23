#include <imgui.h>

#include <gtest/gtest.h>

#include "helper/NcursesScene.hpp"

namespace test
{

class TestNcursesOverlays: public NcursesScene
{
    protected:
        TestNcursesOverlays(): NcursesScene(24, 8) {}

        void tooltip_scene() const
        {
            this->scene_window([] {
                ImGui::Button("hover");
                ImGui::SetItemTooltip("tip");
            });
        }

        void box_select_scene()
        {
            this->scene_window([this] {
                ImGuiMultiSelectIO *requests
                    = ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_BoxSelect2d, selection.Size, 3);
                selection.ApplyRequests(requests);
                for (const char *label : {"a", "b", "c"})
                {
                    const int index = label[0] - 'a';
                    ImGui::SetNextItemSelectionUserData(index);
                    ImGui::Selectable(label, selection.Contains((ImGuiID)index), ImGuiSelectableFlags_None, ImVec2(3, 1));
                }
                selection.ApplyRequests(ImGui::EndMultiSelect());
            });
        }

        ImGuiSelectionBasicStorage selection;
};

TEST_F(TestNcursesOverlays, tooltip_opens_just_below_the_mouse)
{
    ImGuiIO & io = ImGui::GetIO();

    render_frames(2, [&] { tooltip_scene(); });
    io.AddMousePosEvent(2.0f, 1.0f);
    render_frames(60, [&] { tooltip_scene(); });

    EXPECT_EQ(screen_text(),
              "┌──────────────────────┐\n"
              "│ hover                │\n"
              "│    ┌───┐             │\n"
              "│    │tip│             │\n"
              "│    │   │             │\n"
              "│    └───┘             │\n"
              "│                      │\n"
              "└──────────────────────┘");
}

TEST_F(TestNcursesOverlays, main_menu_bar_shows_its_menus)
{
    render_frames(2, [] {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
                ImGui::EndMenu();
            if (ImGui::BeginMenu("Edit"))
                ImGui::EndMenu();
            ImGui::EndMainMenuBar();
        }
    });

    EXPECT_EQ(row_text(0), "─File──Edit─────────────");
}

TEST_F(TestNcursesOverlays, box_selection_rectangle_is_boxed)
{
    ImGuiIO & io = ImGui::GetIO();

    render_frames(2, [&] { box_select_scene(); });
    io.AddMousePosEvent(12.5f, 1.5f);
    render_frames(1, [&] { box_select_scene(); });
    io.AddMouseButtonEvent(0, true);
    render_frames(1, [&] { box_select_scene(); });
    io.AddMousePosEvent(7.5f, 3.0f);
    render_frames(1, [&] { box_select_scene(); });
    io.AddMousePosEvent(2.5f, 4.5f);
    render_frames(1, [&] { box_select_scene(); });

    EXPECT_EQ(screen_text(),
              "┌──────────────────────┐\n"
              "│a┌────────┐           │\n"
              "│b│        │           │\n"
              "│c└────────┘           │\n"
              "│                      │\n"
              "│                      │\n"
              "│                      │\n"
              "└──────────────────────┘");
    EXPECT_EQ(selection.Size, 3);
}

} // namespace test
