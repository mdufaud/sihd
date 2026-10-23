#include <imgui.h>

#include <array>
#include <clocale>

#include <sihd/imgui/ImguiBackendNcurses.hpp>
#include <sihd/imgui/ImguiRendererNcurses.hpp>
#include <sihd/imgui/ImguiRunner.hpp>
#include <sihd/sys/App.hpp>

using namespace sihd::imgui;

namespace
{

struct InputScene
{
        int down = 0;
        int down_frame = 0;
        int tick = 0;
        int alt_x = 0;
        int middle_clicks = 0;
        int right_clicks = 0;
        int backspace = 0;
        int tab = 0;
        int shift_tab = 0;
        int presses = 0;
        int ctrl_presses = 0;
        int far_presses = 0;
        bool checked = false;
        bool animate = false;
        bool focus_text = true;
        bool ready = false;
        bool grip_open = true;
        ImVec2 grip_size {};
        std::array<char, 64> text {};
};

void count_keys(InputScene & scene)
{
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
    {
        ++scene.down;
        scene.down_frame = ImGui::GetFrameCount();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
        ++scene.backspace;
    if (ImGui::IsKeyPressed(ImGuiKey_Tab, false))
        ++(ImGui::GetIO().KeyShift ? scene.shift_tab : scene.tab);
    if (ImGui::IsKeyChordPressed(ImGuiMod_Alt | ImGuiKey_X))
        ++scene.alt_x;
}

void build_input_scene(InputScene & scene)
{
    const ImGuiIO & io = ImGui::GetIO();

    count_keys(scene);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("input",
                 nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                     | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar
                     | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::Text("down=%d backspace=%d tab=%d backtab=%d", scene.down, scene.backspace, scene.tab, scene.shift_tab);
    ImGui::Text("text=[%s]", scene.text.data());
    ImGui::Text("checked=%d presses=%d far=%d ctrl=%d",
                scene.checked,
                scene.presses,
                scene.far_presses,
                scene.ctrl_presses);

    if (scene.focus_text)
    {
        ImGui::SetKeyboardFocusHere();
        scene.focus_text = false;
    }
    ImGui::InputText("##text", scene.text.data(), scene.text.size());
    scene.ready |= ImGui::IsItemActive();
    ImGui::Checkbox("Toggle", &scene.checked);
    if (ImGui::Button("Press"))
    {
        ++scene.presses;
        scene.ctrl_presses += io.KeyCtrl ? 1 : 0;
    }
    ImGui::SetCursorPosX(io.DisplaySize.x - 10.0f);
    if (ImGui::Button("Far"))
        ++scene.far_presses;
    ImGui::Text("grip=%dx%d open=%d held=%d",
                (int)scene.grip_size.x,
                (int)scene.grip_size.y,
                scene.grip_open,
                io.MouseDown[0]);
    ImGui::Text("ready=%d", scene.ready);
    ImGui::Text("down_frame=%d", scene.down_frame);
    ImGui::Checkbox("Animate", &scene.animate);
    if (scene.animate)
        ++scene.tick;
    ImGui::Text("tick=%d", scene.tick);
    scene.middle_clicks += ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ? 1 : 0;
    scene.right_clicks += ImGui::IsMouseClicked(ImGuiMouseButton_Right) ? 1 : 0;
    ImGui::Text("clicks middle=%d right=%d", scene.middle_clicks, scene.right_clicks);
    ImGui::Text("alt_x=%d", scene.alt_x);
    ImGui::End();
}

void build_grip_window(InputScene & scene)
{
    if (!scene.grip_open)
        return;
    ImGui::SetNextWindowPos(ImVec2(40.0f, 18.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(20.0f, 6.0f), ImGuiCond_Once);
    ImGui::Begin("grip", &scene.grip_open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
    scene.grip_size = ImGui::GetWindowSize();
    ImGui::End();
}

} // namespace

int main(int argc, char **argv)
{
    std::setlocale(LC_ALL, "");

    sihd::sys::App app({
        .name = "sihd_imgui_ncurses_scenes",
        .description = "Deterministic imgui ncurses scenes for terminal tests",
    });

    app.root().on_run([&] {
        ImguiRunner imgui("imgui-runner");
        if (!imgui.init_imgui())
            app.exit(EXIT_FAILURE);

        ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        ImguiRendererNcurses renderer;
        renderer.set_clear_color(&clear_color);

        ImguiBackendNcurses backend;
        if (!backend.init() || !renderer.init())
            app.exit(EXIT_FAILURE);

        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().ConfigInputTextCursorBlink = false;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        imgui.set_backend(&backend);
        imgui.set_renderer(&renderer);

        InputScene scene;
        imgui.set_build_frame([&]() -> bool {
            build_input_scene(scene);
            build_grip_window(scene);
            return true;
        });

        imgui.run();
    });

    return app.run(argc, argv);
}
