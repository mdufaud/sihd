#include <sihd/imgui/ImguiBackendAndroid.hpp>
#include <sihd/imgui/ImguiRendererOpenGL.hpp>
#include <sihd/imgui/ImguiRunner.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::util;
using namespace sihd::imgui;

SIHD_NEW_LOGGER("android::main");

void android_main(struct android_app *app)
{
    LoggerManager::stream();

    ImguiBackendAndroid backend;
    backend.set_app(app);

    if (!backend.init_egl())
    {
        SIHD_LOG(error, "Failed to initialize EGL");
        return;
    }

    ImguiRunner imgui("imgui-runner");
    if (!imgui.init_imgui())
    {
        SIHD_LOG(error, "Failed to initialize ImGui");
        return;
    }

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImguiRendererOpenGL opengl_renderer;
    opengl_renderer.set_clear_color(&clear_color);

    if (!backend.init_backend())
    {
        SIHD_LOG(error, "Failed to initialize Android backend");
        return;
    }

    if (!opengl_renderer.init("#version 300 es"))
    {
        SIHD_LOG(error, "Failed to initialize OpenGL renderer");
        return;
    }

    bool show_demo_window = true;
    bool show_another_window = false;
    float slider_value = 0.0f;
    int click_count = 0;

    imgui.set_backend(&backend);
    imgui.set_renderer(&opengl_renderer);
    imgui.set_build_frame([&]() -> bool {
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        {
            ImGui::Begin("Hello, world!");
            ImGui::Text("This is some useful text.");
            ImGui::Checkbox("Demo Window", &show_demo_window);
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &slider_value, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", (float *)&clear_color);

            if (ImGui::Button("Button"))
                click_count++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", click_count);

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
}
