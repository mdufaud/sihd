#include <sihd/imgui/ImguiBackendWin32.hpp>
#include <sihd/imgui/ImguiRendererDirectX.hpp>
#include <sihd/imgui/ImguiRunner.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::util;
using namespace sihd::imgui;

int main(int argc, char **argv)
{
    sihd::sys::App app({
        .name = "imgui_win_d11_demo",
        .description = "Imgui win32 directx11 demo",
    });

    app.root().on_run([&] {
        ImguiRunner imgui("imgui-runner");
        if (!imgui.init_imgui())
            app.exit(EXIT_FAILURE);

        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        ImguiRendererDirectX dx_renderer;
        dx_renderer.set_clear_color(&clear_color);

        ImguiBackendWin32 win32_backend;
        win32_backend.set_resize_renderer(&dx_renderer);

        if (!win32_backend.init_window("Windows Dx11 demo"))
            app.exit(EXIT_FAILURE);
        if (!dx_renderer.init(win32_backend.window()))
            app.exit(EXIT_FAILURE);
        if (!win32_backend.init_backend())
            app.exit(EXIT_FAILURE);

        bool show_demo_window = true;
        bool show_another_window = false;

        imgui.set_backend(&win32_backend);
        imgui.set_renderer(&dx_renderer);
        imgui.set_build_frame([&]() -> bool {
            if (show_demo_window)
                ImGui::ShowDemoWindow(&show_demo_window);

            {
                static float f = 0.0f;
                static int counter = 0;

                ImGui::Begin("Hello, world!");

                ImGui::Text("This is some useful text.");
                ImGui::Checkbox("Demo Window", &show_demo_window);
                ImGui::Checkbox("Another Window", &show_another_window);

                ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
                ImGui::ColorEdit3("clear color", (float *)&clear_color);

                if (ImGui::Button("Button"))
                    counter++;
                ImGui::SameLine();
                ImGui::Text("counter = %d", counter);

                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                            1000.0f / ImGui::GetIO().Framerate,
                            ImGui::GetIO().Framerate);
                ImGui::End();
            }

            if (show_another_window)
            {
                ImGui::Begin("Another Window", &show_another_window);
                ImGui::Text("Hello from another window!");
                if (ImGui::Button("Close Me"))
                    show_another_window = false;
                ImGui::End();
            }
            return true;
        });
        imgui.run();
    });

    return app.run(argc, argv);
}
