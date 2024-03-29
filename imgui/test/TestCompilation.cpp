#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

#include <sihd/imgui/imgui.h>

namespace test
{
SIHD_LOGGER;
// using namespace sihd::imgui;
using namespace sihd::util;
class TestCompilation: public ::testing::Test
{
    protected:
        TestCompilation() { sihd::util::LoggerManager::basic(); }

        virtual ~TestCompilation() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        std::string _cwd;
        std::string _base_test_dir;
};

TEST_F(TestCompilation, test_imgui_demo)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO & io = ImGui::GetIO();

    // Build atlas
    unsigned char *tex_pixels = NULL;
    int tex_w, tex_h;
    io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);
    for (int n = 0; n < 20; n++)
    {
        printf("NewFrame() %d\n", n);
        io.DisplaySize = ImVec2(1920, 1080);
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();

        static float f = 0.0f;
        ImGui::Text("Hello, world!");
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::ShowDemoWindow(NULL);

        ImGui::Render();
    }
    printf("DestroyContext()\n");
    ImGui::DestroyContext();
}

} // namespace test