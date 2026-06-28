#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sihd/imgui/ImguiBackendNcurses.hpp>
#include <sihd/imgui/ImguiRendererNcurses.hpp>
#include <sihd/imgui/ImguiRunner.hpp>
#include <sihd/util/Logger.hpp>

#if defined(__clang__)
# pragma clang diagnostic ignored "-Wold-style-cast"
# pragma clang diagnostic ignored "-Wformat-security"
# pragma clang diagnostic ignored "-Wdouble-promotion"
#elif defined(__GNUC__)
# pragma GCC diagnostic ignored "-Wformat-security"
# pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

using namespace sihd::util;
using namespace sihd::imgui;

static void HelpMarker(const char *desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static void ShowExampleMenuFile()
{
    ImGui::MenuItem("(demo)", nullptr, false, false);
    if (ImGui::MenuItem("New"))
    {
    }
    if (ImGui::MenuItem("Open", "Ctrl+O"))
    {
    }
    if (ImGui::BeginMenu("Open Recent"))
    {
        ImGui::MenuItem("fish_hat.c");
        ImGui::MenuItem("fish_hat.inl");
        ImGui::MenuItem("fish_hat.h");
        if (ImGui::BeginMenu("More.."))
        {
            ImGui::MenuItem("Hello");
            ImGui::MenuItem("Sailor");
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Save", "Ctrl+S"))
    {
    }
    if (ImGui::MenuItem("Save As.."))
    {
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Options"))
    {
        static bool enabled = true;
        ImGui::MenuItem("Enabled", "", &enabled);
        ImGui::BeginChild("child", ImVec2(0, 6), ImGuiChildFlags_Borders);
        for (int i = 0; i < 10; i++)
            ImGui::Text("Scrolling Text %d", i);
        ImGui::EndChild();
        static float f = 0.5f;
        static int n = 0;
        ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
        ImGui::InputFloat("Input", &f, 0.1f);
        ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Colors"))
    {
        float sz = ImGui::GetTextLineHeight();
        for (int i = 0; i < ImGuiCol_COUNT; i++)
        {
            const char *name = ImGui::GetStyleColorName((ImGuiCol)i);
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), ImGui::GetColorU32((ImGuiCol)i));
            ImGui::Dummy(ImVec2(sz, sz));
            ImGui::SameLine();
            ImGui::MenuItem(name);
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Quit", "Alt+F4"))
    {
    }
}

static bool _debug_open_all()
{
    static const bool v = std::getenv("SIHD_IMGUI_NCURSES_OPENALL") != nullptr;
    return v;
}

static void ShowDemoWindowWidgets()
{
    if (_debug_open_all())
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    if (!ImGui::CollapsingHeader("Widgets"))
        return;

    if (_debug_open_all())
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    if (ImGui::TreeNode("Basic"))
    {
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.33f);

        static int clicked = 0;
        if (ImGui::Button("Button"))
            clicked++;
        if (clicked & 1)
        {
            ImGui::SameLine();
            ImGui::Text("Thanks for clicking me!");
        }

        static bool check = true;
        ImGui::Checkbox("checkbox", &check);

        static int e = 0;
        ImGui::RadioButton("radio a", &e, 0);
        ImGui::SameLine();
        ImGui::RadioButton("radio b", &e, 1);
        ImGui::SameLine();
        ImGui::RadioButton("radio c", &e, 2);

        for (int i = 0; i < 7; i++)
        {
            if (i > 0)
                ImGui::SameLine();
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(i / 7.0f, 0.6f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(i / 7.0f, 0.7f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(i / 7.0f, 0.8f, 0.8f));
            ImGui::Button("Click");
            ImGui::PopStyleColor(3);
            ImGui::PopID();
        }

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Hold to repeat:");
        ImGui::SameLine();

        static int counter = 0;
        float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::PushButtonRepeat(true);
        if (ImGui::ArrowButton("##left", ImGuiDir_Left))
        {
            counter--;
        }
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::ArrowButton("##right", ImGuiDir_Right))
        {
            counter++;
        }
        ImGui::PopButtonRepeat();
        ImGui::SameLine();
        ImGui::Text("%d", counter);

        ImGui::Text("Hover over me");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("I am a tooltip");

        ImGui::SameLine();
        ImGui::Text("- or me");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("I am a fancy tooltip");
            static float arr[] = {0.6f, 0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f};
            ImGui::PlotLines("Curve", arr, IM_ARRAYSIZE(arr));
            ImGui::EndTooltip();
        }

        ImGui::Dummy(ImVec2(0, 1));

        ImGui::LabelText("label", "Value");

        {
            const char *items[] = {"AAAA",
                                   "BBBB",
                                   "CCCC",
                                   "DDDD",
                                   "EEEE",
                                   "FFFF",
                                   "GGGG",
                                   "HHHH",
                                   "IIII",
                                   "JJJJ",
                                   "KKKK",
                                   "LLLLLLL",
                                   "MMMM",
                                   "OOOOOOO"};
            static int item_current = 0;
            ImGui::Combo("combo", &item_current, items, IM_ARRAYSIZE(items));
            ImGui::SameLine();
            HelpMarker(
                "Refer to the \"Combo\" section below for an explanation of the full BeginCombo/EndCombo API, and demonstration of various flags.\n");
        }

        {
            static char str0[128] = "Hello, world!";
            ImGui::InputText("input text", str0, IM_ARRAYSIZE(str0));

            ImGui::Dummy(ImVec2(0, 1));

            static char str1[128] = "";
            ImGui::InputTextWithHint("input text (w/ hint)", "enter text here", str1, IM_ARRAYSIZE(str1));

            ImGui::Dummy(ImVec2(0, 1));

            static int i0 = 123;
            ImGui::InputInt("input int", &i0);

            ImGui::Dummy(ImVec2(0, 1));

            static float f0 = 0.001f;
            ImGui::InputFloat("input float", &f0, 0.01f, 1.0f, "%.3f");

            ImGui::Dummy(ImVec2(0, 1));

            static double d0 = 999999.00000001;
            ImGui::InputDouble("input double", &d0, 0.01f, 1.0f, "%.8f");

            ImGui::Dummy(ImVec2(0, 1));

            static float f1 = 1.e10f;
            ImGui::InputFloat("input scientific", &f1, 0.0f, 0.0f, "%e");

            ImGui::Dummy(ImVec2(0, 1));

            static float vec4a[4] = {0.10f, 0.20f, 0.30f, 0.44f};
            ImGui::InputFloat3("input float3", vec4a);
        }

        ImGui::Dummy(ImVec2(0, 1));

        {
            static int i1 = 50, i2 = 42;
            ImGui::DragInt("drag int", &i1, 1);
            ImGui::SameLine();
            HelpMarker(
                "Click and drag to edit value.\nHold SHIFT/ALT for faster/slower edit.\nDouble-click or CTRL+click to input value.");

            ImGui::Dummy(ImVec2(0, 1));

            ImGui::DragInt("drag int 0..100", &i2, 1, 0, 100, "%d%%");

            ImGui::Dummy(ImVec2(0, 1));

            static float f1 = 1.00f, f2 = 0.0067f;
            ImGui::DragFloat("drag float", &f1, 0.005f);

            ImGui::Dummy(ImVec2(0, 1));

            ImGui::DragFloat("drag small float", &f2, 0.0001f, 0.0f, 0.0f, "%.06f ns");
        }

        ImGui::Dummy(ImVec2(0, 1));

        {
            static int i1 = 0;
            ImGui::SliderInt("slider int", &i1, -1, 3);
            ImGui::SameLine();
            HelpMarker("CTRL+click to input value.");

            ImGui::Dummy(ImVec2(0, 1));

            static float f1 = 0.123f, f2 = 0.0f;
            ImGui::SliderFloat("slider float", &f1, 0.0f, 1.0f, "ratio = %.3f");
            ImGui::SliderFloat("slider float (curve)", &f2, -10.0f, 10.0f, "%.4f", ImGuiSliderFlags_Logarithmic);

            ImGui::Dummy(ImVec2(0, 1));

            static float angle = 0.0f;
            ImGui::SliderAngle("slider angle", &angle);

            ImGui::Dummy(ImVec2(0, 1));

            enum Element
            {
                Element_Fire,
                Element_Earth,
                Element_Air,
                Element_Water,
                Element_COUNT
            };
            const char *element_names[Element_COUNT] = {"Fire", "Earth", "Air", "Water"};
            static int current_element = Element_Fire;
            const char *current_element_name = (current_element >= 0 && current_element < Element_COUNT)
                                                   ? element_names[current_element]
                                                   : "Unknown";
            ImGui::SliderInt("slider enum", &current_element, 0, Element_COUNT - 1, current_element_name);
            ImGui::SameLine();
            HelpMarker("Using the format string parameter to display a name instead of the underlying integer.");
        }

        ImGui::Dummy(ImVec2(0, 1));

        {
            static float col1[3] = {1.0f, 0.0f, 0.2f};
            static float col2[4] = {0.4f, 0.7f, 0.0f, 0.5f};
            ImGui::ColorEdit3("color 1", col1);
            ImGui::SameLine();
            HelpMarker(
                "Click on the colored square to open a color picker.\nClick and hold to use drag and drop.\nRight-click on the colored square to show options.\nCTRL+click on individual component to input value.\n");
            ImGui::ColorEdit4("color 2", col2);
        }

        ImGui::Dummy(ImVec2(0, 1));

        {
            const char *listbox_items[] =
                {"Apple", "Banana", "Cherry", "Kiwi", "Mango", "Orange", "Pineapple", "Strawberry", "Watermelon"};
            static int listbox_item_current = 1;
            ImGui::ListBox("listbox\n(single select)",
                           &listbox_item_current,
                           listbox_items,
                           IM_ARRAYSIZE(listbox_items),
                           4);
        }

        ImGui::PopItemWidth();
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Trees"))
    {
        if (ImGui::TreeNode("Basic trees"))
        {
            for (int i = 0; i < 5; i++)
            {
                if (i == 0)
                    ImGui::SetNextItemOpen(true, ImGuiCond_Once);

                if (ImGui::TreeNode((void *)(intptr_t)i, "Child %d", i))
                {
                    ImGui::Text("blah blah");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("button"))
                    {
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Advanced, with Selectable nodes"))
        {
            HelpMarker(
                "This is a more typical looking tree with selectable nodes.\nClick to select, CTRL+Click to toggle, click on arrows or double-click to open.");
            static ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                                                   | ImGuiTreeNodeFlags_SpanAvailWidth;
            static bool align_label_with_current_x_position = false;
            ImGui::CheckboxFlags("ImGuiTreeNodeFlags_OpenOnArrow", &base_flags, ImGuiTreeNodeFlags_OpenOnArrow);
            ImGui::CheckboxFlags("ImGuiTreeNodeFlags_OpenOnDoubleClick",
                                 &base_flags,
                                 ImGuiTreeNodeFlags_OpenOnDoubleClick);
            ImGui::CheckboxFlags("ImGuiTreeNodeFlags_SpanAvailWidth", &base_flags, ImGuiTreeNodeFlags_SpanAvailWidth);
            ImGui::CheckboxFlags("ImGuiTreeNodeFlags_SpanFullWidth", &base_flags, ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::Checkbox("Align label with current X position", &align_label_with_current_x_position);
            ImGui::Text("Hello!");
            if (align_label_with_current_x_position)
                ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());

            static int selection_mask = (1 << 2);
            int node_clicked = -1;
            for (int i = 0; i < 6; i++)
            {
                ImGuiTreeNodeFlags node_flags = base_flags;
                const bool is_selected = (selection_mask & (1 << i)) != 0;
                if (is_selected)
                    node_flags |= ImGuiTreeNodeFlags_Selected;
                if (i < 3)
                {
                    bool node_open = ImGui::TreeNodeEx((void *)(intptr_t)i, node_flags, "Selectable Node %d", i);
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                        node_clicked = i;
                    if (node_open)
                    {
                        ImGui::BulletText("Blah blah\nBlah Blah");
                        ImGui::TreePop();
                    }
                }
                else
                {
                    node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    ImGui::TreeNodeEx((void *)(intptr_t)i, node_flags, "Selectable Leaf %d", i);
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                        node_clicked = i;
                }
            }
            if (node_clicked != -1)
            {
                if (ImGui::GetIO().KeyCtrl)
                    selection_mask ^= (1 << node_clicked);
                else
                    selection_mask = (1 << node_clicked);
            }
            if (align_label_with_current_x_position)
                ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Collapsing Headers"))
    {
        static bool closable_group = true;
        ImGui::Checkbox("Show 2nd header", &closable_group);
        if (ImGui::CollapsingHeader("Header", ImGuiTreeNodeFlags_None))
        {
            ImGui::Text("IsItemHovered: %d", ImGui::IsItemHovered());
            for (int i = 0; i < 5; i++)
                ImGui::Text("Some content %d", i);
        }
        if (ImGui::CollapsingHeader("Header with a close button", &closable_group))
        {
            ImGui::Text("IsItemHovered: %d", ImGui::IsItemHovered());
            for (int i = 0; i < 5; i++)
                ImGui::Text("More content %d", i);
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Bullets"))
    {
        ImGui::BulletText("Bullet point 1");
        ImGui::BulletText("Bullet point 2\nOn multiple lines");
        if (ImGui::TreeNode("Tree node"))
        {
            ImGui::BulletText("Another bullet point");
            ImGui::TreePop();
        }
        ImGui::Bullet();
        ImGui::Text("Bullet point 3 (two calls)");
        ImGui::Bullet();
        ImGui::SmallButton("Button");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Text"))
    {
        if (ImGui::TreeNode("Colored Text"))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "Pink");
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Yellow");
            ImGui::TextDisabled("Disabled");
            ImGui::SameLine();
            HelpMarker("The TextDisabled color is stored in ImGuiStyle.");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Word Wrapping"))
        {
            ImGui::TextWrapped(
                "This text should automatically wrap on the edge of the window. The current implementation for text wrapping follows simple rules suitable for English and possibly other languages.");
            ImGui::Spacing();

            static float wrap_width = 60.0f;
            ImGui::SliderFloat("Wrap width", &wrap_width, -20, 600, "%.0f");

            ImGui::Text("Test paragraph 1:");
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 marker_min = ImVec2(pos.x + wrap_width, pos.y);
            ImVec2 marker_max = ImVec2(pos.x + wrap_width + 1, pos.y + ImGui::GetTextLineHeight());
            ImGui::GetWindowDrawList()->AddRectFilled(marker_min, marker_max, IM_COL32(255, 0, 255, 255));
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_width);
            ImGui::Text(
                "The lazy dog is a good dog. This paragraph is made to fit within %.0f pixels. Testing a 1 character word. The quick brown fox jumps over the lazy dog.",
                wrap_width);
            ImGui::PopTextWrapPos();

            ImGui::Text("Test paragraph 2:");
            pos = ImGui::GetCursorScreenPos();
            marker_min = ImVec2(pos.x + wrap_width, pos.y);
            marker_max = ImVec2(pos.x + wrap_width + 1, pos.y + ImGui::GetTextLineHeight());
            ImGui::GetWindowDrawList()->AddRectFilled(marker_min, marker_max, IM_COL32(255, 0, 255, 255));
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_width);
            ImGui::Text("aaaaaaaa bbbbbbbb, c cccccccc,dddddddd. d eeeeeeee   ffffffff. gggggggg!hhhhhhhh");
            ImGui::PopTextWrapPos();

            ImGui::TreePop();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Combo"))
    {
        static ImGuiComboFlags flags = 0;
        ImGui::CheckboxFlags("ImGuiComboFlags_PopupAlignLeft", &flags, ImGuiComboFlags_PopupAlignLeft);
        ImGui::SameLine();
        HelpMarker("Only makes a difference if the popup is larger than the combo");
        if (ImGui::CheckboxFlags("ImGuiComboFlags_NoArrowButton", &flags, ImGuiComboFlags_NoArrowButton))
            flags &= ~ImGuiComboFlags_NoPreview;
        if (ImGui::CheckboxFlags("ImGuiComboFlags_NoPreview", &flags, ImGuiComboFlags_NoPreview))
            flags &= ~ImGuiComboFlags_NoArrowButton;

        const char *items[] = {"AAAA",
                               "BBBB",
                               "CCCC",
                               "DDDD",
                               "EEEE",
                               "FFFF",
                               "GGGG",
                               "HHHH",
                               "IIII",
                               "JJJJ",
                               "KKKK",
                               "LLLLLLL",
                               "MMMM",
                               "OOOOOOO"};
        static const char *item_current = items[0];
        if (ImGui::BeginCombo("combo 1", item_current, flags))
        {
            for (int n = 0; n < IM_ARRAYSIZE(items); n++)
            {
                bool is_selected = (item_current == items[n]);
                if (ImGui::Selectable(items[n], is_selected))
                    item_current = items[n];
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        static int item_current_2 = 0;
        ImGui::Combo("combo 2 (one-liner)", &item_current_2, "aaaa\0bbbb\0cccc\0dddd\0eeee\0\0");

        static int item_current_3 = -1;
        ImGui::Combo("combo 3 (array)", &item_current_3, items, IM_ARRAYSIZE(items));

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Selectables"))
    {
        if (ImGui::TreeNode("Basic"))
        {
            static bool selection[5] = {false, true, false, false, false};
            ImGui::Selectable("1. I am selectable", &selection[0]);
            ImGui::Selectable("2. I am selectable", &selection[1]);
            ImGui::Text("3. I am not selectable");
            ImGui::Selectable("4. I am selectable", &selection[3]);
            if (ImGui::Selectable("5. I am double clickable", selection[4], ImGuiSelectableFlags_AllowDoubleClick))
                if (ImGui::IsMouseDoubleClicked(0))
                    selection[4] = !selection[4];
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Selection State: Single Selection"))
        {
            static int selected = -1;
            for (int n = 0; n < 5; n++)
            {
                char buf[32];
                sprintf(buf, "Object %d", n);
                if (ImGui::Selectable(buf, selected == n))
                    selected = n;
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Selection State: Multiple Selection"))
        {
            HelpMarker("Hold CTRL and click to select multiple items.");
            static bool selection[5] = {false, false, false, false, false};
            for (int n = 0; n < 5; n++)
            {
                char buf[32];
                sprintf(buf, "Object %d", n);
                if (ImGui::Selectable(buf, selection[n]))
                {
                    if (!ImGui::GetIO().KeyCtrl)
                        memset(selection, 0, sizeof(selection));
                    selection[n] ^= 1;
                }
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("In columns"))
        {
            ImGui::Columns(3, nullptr, false);
            static bool selected2[16] = {};
            for (int i = 0; i < 16; i++)
            {
                char label[32];
                sprintf(label, "Item %d", i);
                if (ImGui::Selectable(label, &selected2[i]))
                {
                }
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Grid"))
        {
            static bool selected3[4 * 4] = {true,
                                            false,
                                            false,
                                            false,
                                            false,
                                            true,
                                            false,
                                            false,
                                            false,
                                            false,
                                            true,
                                            false,
                                            false,
                                            false,
                                            false,
                                            true};
            for (int i = 0; i < 4 * 4; i++)
            {
                ImGui::PushID(i);
                if (ImGui::Selectable("Sailor", &selected3[i], 0, ImVec2(8, 4)))
                {
                    int x = i % 4;
                    int y = i / 4;
                    if (x > 0)
                    {
                        selected3[i - 1] ^= 1;
                    }
                    if (x < 3 && i < 15)
                    {
                        selected3[i + 1] ^= 1;
                    }
                    if (y > 0 && i > 3)
                    {
                        selected3[i - 4] ^= 1;
                    }
                    if (y < 3 && i < 12)
                    {
                        selected3[i + 4] ^= 1;
                    }
                }
                if ((i % 4) < 3)
                    ImGui::SameLine();
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Alignment"))
        {
            HelpMarker(
                "Alignment applies when a selectable is larger than its text content.\nBy default, Selectables uses style.SelectableTextAlign but it can be overridden on a per-item basis using PushStyleVar().");
            static bool selected4[3 * 3] = {true, false, true, false, true, false, true, false, true};
            for (int y = 0; y < 3; y++)
            {
                for (int x = 0; x < 3; x++)
                {
                    ImVec2 alignment = ImVec2((float)x / 2.0f, (float)y / 2.0f);
                    char name[32];
                    sprintf(name, "(%.1f,%.1f)", alignment.x, alignment.y);
                    if (x > 0)
                        ImGui::SameLine();
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, alignment);
                    ImGui::Selectable(name, &selected4[3 * y + x], ImGuiSelectableFlags_None, ImVec2(8, 4));
                    ImGui::PopStyleVar();
                }
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Text Input"))
    {
        if (ImGui::TreeNode("Multi-line Text Input"))
        {
            static char text[1024 * 16] = "/*\n"
                                          " The Pentium F00F bug, shorthand for F0 0F C7 C8,\n"
                                          " the hexadecimal encoding of one offending instruction,\n"
                                          " more formally, the invalid operand with locked CMPXCHG8B\n"
                                          " instruction bug, is a design flaw in the majority of\n"
                                          " Intel Pentium, Pentium MMX, and Pentium OverDrive\n"
                                          " processors (all in the P5 microarchitecture).\n"
                                          "*/\n\n"
                                          "label:\n"
                                          "\tlock cmpxchg8b eax\n";

            static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
            HelpMarker(
                "You can use the ImGuiInputTextFlags_CallbackResize facility if you need to wire InputTextMultiline() to a dynamic string type. See misc/cpp/imgui_stdlib.h for an example. (This is not demonstrated in imgui_demo.cpp)");
            ImGui::CheckboxFlags("ImGuiInputTextFlags_ReadOnly", &flags, ImGuiInputTextFlags_ReadOnly);
            ImGui::CheckboxFlags("ImGuiInputTextFlags_AllowTabInput", &flags, ImGuiInputTextFlags_AllowTabInput);
            ImGui::CheckboxFlags("ImGuiInputTextFlags_CtrlEnterForNewLine",
                                 &flags,
                                 ImGuiInputTextFlags_CtrlEnterForNewLine);
            ImGui::InputTextMultiline("##source",
                                      text,
                                      IM_ARRAYSIZE(text),
                                      ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16),
                                      flags);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Filtered Text Input"))
        {
            constexpr size_t term_buf_size = 32;
            static char buf1[term_buf_size] = "";
            ImGui::InputText("default", buf1, term_buf_size);
            static char buf2[term_buf_size] = "";
            ImGui::InputText("decimal", buf2, term_buf_size, ImGuiInputTextFlags_CharsDecimal);
            static char buf3[term_buf_size] = "";
            ImGui::InputText("hexadecimal",
                             buf3,
                             term_buf_size,
                             ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
            static char buf4[term_buf_size] = "";
            ImGui::InputText("uppercase", buf4, term_buf_size, ImGuiInputTextFlags_CharsUppercase);
            static char buf5[term_buf_size] = "";
            ImGui::InputText("no blank", buf5, term_buf_size, ImGuiInputTextFlags_CharsNoBlank);

            ImGui::Text("Password input");
            static char bufpass[term_buf_size] = "password123";
            ImGui::InputText("password",
                             bufpass,
                             term_buf_size,
                             ImGuiInputTextFlags_Password | ImGuiInputTextFlags_CharsNoBlank);
            ImGui::SameLine();
            HelpMarker("Display all characters as '*'.\nDisable clipboard cut and copy.\nDisable logging.\n");
            ImGui::InputText("password (clear)", bufpass, term_buf_size, ImGuiInputTextFlags_CharsNoBlank);
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }

    if (_debug_open_all())
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    if (ImGui::TreeNode("Plots Widgets"))
    {
        static bool animate = true;
        ImGui::Checkbox("Animate", &animate);

        static float arr[] = {
            0.6f,  0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f, 0.2f,  0.7f, 1.5f, 0.5f,
            0.32f, 0.3f, 0.2f, 0.3f, 0.3f,  1.3f, 0.5f, 0.92f, 0.5f, 0.2f,
        };

        static float values[90] = {};
        static int values_offset = 0;
        static double refresh_time = 0.0;
        if (!animate || refresh_time == 0.0)
            refresh_time = ImGui::GetTime();
        while (refresh_time < ImGui::GetTime())
        {
            static float phase = 0.0f;
            values[values_offset] = cosf(phase);
            values_offset = (values_offset + 1) % IM_ARRAYSIZE(values);
            phase += 0.0010f * values_offset;
            refresh_time += 1.0f / 60.0f;
        }

        {
            float average = 0.0f;
            for (int n = 0; n < IM_ARRAYSIZE(values); n++)
                average += values[n];
            average /= (float)IM_ARRAYSIZE(values);
            char overlay[32];
            sprintf(overlay, "avg %f", average);
            ImGui::Text("Lines");
            ImGui::PlotLines("##lines",
                             values,
                             IM_ARRAYSIZE(values),
                             values_offset,
                             overlay,
                             -1.0f,
                             1.0f,
                             ImVec2(0, 8));
        }
        ImGui::TextUnformatted("");
        ImGui::Text("Histogram");
        ImGui::PlotHistogram("##histogram", arr, IM_ARRAYSIZE(arr), 0, nullptr, 0.0f, 1.5f, ImVec2(0, 16));

        struct Funcs
        {
                static float Sin(void *, int i) { return sinf(i * 0.1f); }
                static float Saw(void *, int i) { return (i & 1) ? 1.0f : -1.0f; }
        };
        static int func_type = 0, display_count = 70;

        ImGui::Dummy(ImVec2(0, 1));
        ImGui::SetNextItemWidth(20.0f);
        ImGui::Combo("func", &func_type, "Sin\0Saw\0");

        ImGui::Dummy(ImVec2(0, 1));
        ImGui::SetNextItemWidth(40.0f);
        ImGui::SliderInt("Sample count", &display_count, 1, 200);

        float (*func)(void *, int) = (func_type == 0) ? Funcs::Sin : Funcs::Saw;
        ImGui::TextUnformatted("");
        ImGui::Text("Lines");
        ImGui::PlotLines("##lines_func", func, nullptr, display_count, 0, nullptr, -1.0f, 1.0f, ImVec2(0, 12));
        ImGui::TextUnformatted("");
        ImGui::Text("Histogram");
        ImGui::PlotHistogram("##histogram_func", func, nullptr, display_count, 0, nullptr, -1.0f, 1.0f, ImVec2(0, 12));
        ImGui::Dummy(ImVec2(0, 1));

        static float progress = 0.0f, progress_dir = 1.0f;
        if (animate)
        {
            progress += progress_dir * 0.4f * ImGui::GetIO().DeltaTime;
            if (progress >= +1.1f)
            {
                progress = +1.1f;
                progress_dir *= -1.0f;
            }
            if (progress <= -0.1f)
            {
                progress = -0.1f;
                progress_dir *= -1.0f;
            }
        }

        ImGui::TextUnformatted("");
        ImGui::Text("Progress Bar");
        ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f));
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

        ImGui::Dummy(ImVec2(0, 1));

        ImGui::TextUnformatted("");
        float progress_saturated = (progress < 0.0f) ? 0.0f : (progress > 1.0f) ? 1.0f : progress;
        char buf[32];
        sprintf(buf, "%d/%d", (int)(progress_saturated * 1753), 1753);
        ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), buf);
        ImGui::TreePop();
    }
}

static void ShowDemoWindowLayout()
{
    if (!ImGui::CollapsingHeader("Layout & Scrolling"))
        return;

    if (ImGui::TreeNode("Child windows"))
    {
        HelpMarker(
            "Use child windows to begin into a self-contained independent scrolling/clipping regions within a host window.");
        static bool disable_mouse_wheel = false;
        static bool disable_menu = false;
        ImGui::Checkbox("Disable Mouse Wheel", &disable_mouse_wheel);
        ImGui::Checkbox("Disable Menu", &disable_menu);

        static int line = 50;
        bool goto_line = ImGui::Button("Goto");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        goto_line |= ImGui::InputInt("##Line", &line, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);

        {
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar
                                            | (disable_mouse_wheel ? ImGuiWindowFlags_NoScrollWithMouse : 0);
            ImGui::BeginChild("Child1",
                              ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 15),
                              ImGuiChildFlags_Borders,
                              window_flags);
            for (int i = 0; i < 100; i++)
            {
                ImGui::Text("%04d: scrollable region", i);
                if (goto_line && line == i)
                    ImGui::SetScrollHereY();
            }
            if (goto_line && line >= 100)
                ImGui::SetScrollHereY();
            ImGui::EndChild();
        }

        ImGui::SameLine();

        {
            ImGuiWindowFlags window_flags = (disable_mouse_wheel ? ImGuiWindowFlags_NoScrollWithMouse : 0)
                                            | (disable_menu ? 0 : ImGuiWindowFlags_MenuBar);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
            ImGui::BeginChild("Child2", ImVec2(0, 15), ImGuiChildFlags_Borders, window_flags);
            if (!disable_menu && ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("Menu"))
                {
                    ShowExampleMenuFile();
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
            if (ImGui::BeginTable("split", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings))
            {
                for (int i = 0; i < 100; i++)
                {
                    char buf[32];
                    sprintf(buf, "%03d", i);
                    ImGui::TableNextColumn();
                    ImGui::Button(buf, ImVec2(-FLT_MIN, 0.0f));
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Widgets Width"))
    {
        static float f = 0.0f;
        ImGui::Text("SetNextItemWidth/PushItemWidth(100)");
        ImGui::SameLine();
        HelpMarker("Fixed width.");
        ImGui::PushItemWidth(100);
        ImGui::DragFloat("float##1b", &f);
        ImGui::PopItemWidth();

        ImGui::Text("SetNextItemWidth/PushItemWidth(-100)");
        ImGui::SameLine();
        HelpMarker("Align to right edge minus 100");
        ImGui::PushItemWidth(-100);
        ImGui::DragFloat("float##2a", &f);
        ImGui::PopItemWidth();

        ImGui::Text("SetNextItemWidth/PushItemWidth(GetContentRegionAvail().x * 0.5f)");
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        ImGui::DragFloat("float##3a", &f);
        ImGui::PopItemWidth();

        ImGui::Text("SetNextItemWidth/PushItemWidth(-1)");
        ImGui::SameLine();
        HelpMarker("Align to right edge");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat("float##4a", &f);
        ImGui::PopItemWidth();

        ImGui::TreePop();
    }
}

static void ShowDemoWindowPopups()
{
    if (!ImGui::CollapsingHeader("Popups & Modal windows"))
        return;

    if (ImGui::TreeNode("Popups"))
    {
        ImGui::TextWrapped(
            "When a popup is active, it inhibits interacting with windows that are behind the popup. Clicking outside the popup closes it.");

        static int selected_fish = -1;
        const char *names[] = {"Bream", "Haddock", "Mackerel", "Pollock", "Tilefish"};
        static bool toggles[] = {true, false, false, false, false};

        if (ImGui::Button("Select.."))
            ImGui::OpenPopup("my_select_popup");
        ImGui::SameLine();
        ImGui::TextUnformatted(selected_fish == -1 ? "<None>" : names[selected_fish]);
        if (ImGui::BeginPopup("my_select_popup"))
        {
            ImGui::Text("Aquarium");
            ImGui::Separator();
            for (int i = 0; i < IM_ARRAYSIZE(names); i++)
                if (ImGui::Selectable(names[i]))
                    selected_fish = i;
            ImGui::EndPopup();
        }

        if (ImGui::Button("Toggle.."))
            ImGui::OpenPopup("my_toggle_popup");
        if (ImGui::BeginPopup("my_toggle_popup"))
        {
            for (int i = 0; i < IM_ARRAYSIZE(names); i++)
                ImGui::MenuItem(names[i], "", &toggles[i]);
            if (ImGui::BeginMenu("Sub-menu"))
            {
                ImGui::MenuItem("Click me");
                ImGui::EndMenu();
            }
            ImGui::Separator();
            ImGui::Text("Tooltip here");
            ImGui::SetItemTooltip("I am a tooltip over a popup");

            if (ImGui::Button("Stacked Popup"))
                ImGui::OpenPopup("another popup");
            if (ImGui::BeginPopup("another popup"))
            {
                for (int i = 0; i < IM_ARRAYSIZE(names); i++)
                    ImGui::MenuItem(names[i], "", &toggles[i]);
                if (ImGui::BeginMenu("Sub-menu"))
                {
                    ImGui::MenuItem("Click me");
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Context menus"))
    {
        {
            const char *names[5] = {"Label1", "Label2", "Label3", "Label4", "Label5"};
            static int selected = -1;
            for (int n = 0; n < 5; n++)
            {
                if (ImGui::Selectable(names[n], selected == n))
                    selected = n;
                if (ImGui::BeginPopupContextItem())
                {
                    selected = n;
                    ImGui::Text("This a popup for \"%s\"!", names[n]);
                    if (ImGui::Button("Close"))
                        ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
                ImGui::SetItemTooltip("Right-click to open popup");
            }
        }

        {
            static float value = 0.5f;
            ImGui::Text("Value = %.3f (<-- right-click here)", value);
            if (ImGui::BeginPopupContextItem("my popup"))
            {
                if (ImGui::Selectable("Set to zero"))
                    value = 0.0f;
                if (ImGui::Selectable("Set to PI"))
                    value = 3.1415f;
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##Value", &value, 0.1f, 0.0f, 0.0f);
                ImGui::EndPopup();
            }
            ImGui::Text("(2) Or right-click this text");
            ImGui::OpenPopupOnItemClick("my popup", ImGuiPopupFlags_MouseButtonRight);
            if (ImGui::Button("(3) Or click this button"))
                ImGui::OpenPopup("my popup");
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Modals"))
    {
        ImGui::TextWrapped("Modal windows are like popups but the user cannot close them by clicking outside.");

        if (ImGui::Button("Delete.."))
            ImGui::OpenPopup("Delete?");

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Delete?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("All those beautiful files will be deleted.");
            ImGui::Text("This operation cannot be undone!");
            ImGui::Separator();

            static bool dont_ask_me_next_time = false;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::Checkbox("Don't ask me next time", &dont_ask_me_next_time);
            ImGui::PopStyleVar();

            if (ImGui::Button("OK", ImVec2(12, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(12, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (ImGui::Button("Stacked modals.."))
            ImGui::OpenPopup("Stacked 1");
        if (ImGui::BeginPopupModal("Stacked 1", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Some menu item"))
                    {
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
            ImGui::Text("Hello from Stacked The First");

            static int item = 1;
            ImGui::Combo("Combo", &item, "aaaa\0bbbb\0cccc\0dddd\0eeee\0\0");

            if (ImGui::Button("Add another modal.."))
                ImGui::OpenPopup("Stacked 2");
            bool unused_open = true;
            if (ImGui::BeginPopupModal("Stacked 2", &unused_open))
            {
                ImGui::Text("Hello from Stacked The Second!");
                if (ImGui::Button("Close"))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::TreePop();
    }
}

static void ShowDemoWindow()
{
    static bool show_app_about = false;

    if (show_app_about)
        ImGui::ShowAboutWindow(&show_app_about);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;

    ImGui::SetNextWindowPos(ImVec2(4, 2), ImGuiCond_FirstUseEver);
    if (_debug_open_all())
        ImGui::SetNextWindowSize(ImVec2(140, 90), ImGuiCond_Always);
    else
        ImGui::SetNextWindowSize(ImVec2(70, 24), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("sihd ncurses Demo (powered by Dear ImGui)", nullptr, window_flags))
    {
        ImGui::End();
        return;
    }

    ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Menu"))
        {
            ShowExampleMenuFile();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            ImGui::MenuItem("About Dear ImGui", nullptr, &show_app_about);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::Text("dear imgui says hello. (%s)", IMGUI_VERSION);
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Help"))
    {
        ImGui::Text("ABOUT THIS DEMO:");
        ImGui::BulletText("Sections below are demonstrating many aspects of the library.");
        ImGui::BulletText("The \"Tools\" menu above gives access to About Box.");
        ImGui::Separator();

        ImGui::Text("PROGRAMMER GUIDE:");
        ImGui::BulletText("See the ShowDemoWindow() code in imgui_demo.cpp.");
        ImGui::BulletText("See comments in imgui.cpp.");
        ImGui::BulletText("See example applications in the examples/ folder.");
        ImGui::BulletText("Read the FAQ at http://www.dearimgui.org/faq/");
        ImGui::BulletText("Set 'io.ConfigFlags |= NavEnableKeyboard' for keyboard controls.");
    }

    ShowDemoWindowWidgets();
    ShowDemoWindowLayout();
    ShowDemoWindowPopups();

    ImGui::PopItemWidth();
    ImGui::End();
}

int main()
{
    LoggerManager::stream();

    ImguiRunner imgui("imgui-runner");
    if (!imgui.init_imgui())
        return 1;

    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    ImguiRendererNcurses ncurses_renderer;
    ncurses_renderer.set_clear_color(&clear_color);

    ImguiBackendNcurses ncurses_backend;
    if (!ncurses_backend.init())
        return 1;

    if (!ncurses_renderer.init())
        return 1;

    ImGui::GetIO().IniFilename = nullptr;

    imgui.set_backend(&ncurses_backend);
    imgui.set_renderer(&ncurses_renderer);

    int nframes = 0;
    float fval = 1.23f;

    imgui.set_build_frame([&]() -> bool {
        const ImGuiIO & io = ImGui::GetIO();

        ImGui::SetNextWindowPos(ImVec2(4, 27), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(50.0, 10.0), ImGuiCond_Once);
        ImGui::Begin("Hello, world!");
        ImGui::Text("NFrames = %d", nframes++);
        ImGui::Text("Mouse Pos : x = %g, y = %g", io.MousePos.x, io.MousePos.y);
        ImGui::Text("Time per frame %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::Text("Float:");
        ImGui::SameLine();
        ImGui::SliderFloat("##float", &fval, 0.0f, 10.0f);

        ImGui::TextUnformatted("");
        if (ImGui::Button("Exit program", {ImGui::GetContentRegionAvail().x, 2}))
        {
            ImGui::End();
            return false;
        }

        ImGui::End();

        ShowDemoWindow();

        return true;
    });

    imgui.run();
    return 0;
}
