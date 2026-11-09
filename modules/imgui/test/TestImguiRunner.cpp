#include <imgui.h>

#include <gtest/gtest.h>

#include <sihd/imgui/ImguiRunner.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::imgui;
using namespace sihd::util;

namespace
{

class FakeBackend: public IImguiBackend
{
    public:
        void new_frame() override {}
        bool should_close() override { return false; }
        void poll() override {}
        void pre_render() override {}
        void post_render() override {}
        void shutdown() override {}
        void terminate() override {}
};

class FakeRenderer: public IImguiRenderer
{
    public:
        void new_frame() override {}
        void render(ImDrawData *) override {}
        void shutdown() override {}
        void resize() override {}
        void set_clear_color(ImVec4 *) override {}
        ImVec4 *get_clear_color() override { return nullptr; }
};

} // namespace

class TestImguiRunner: public ::testing::Test
{
    protected:
        TestImguiRunner() { LoggerManager::stream(); }

        virtual ~TestImguiRunner() { LoggerManager::clear_loggers(); }

        // the gui closes itself on its third frame
        void run_until_the_gui_closes()
        {
            ASSERT_TRUE(_runner.init_imgui());
            ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
            ImGui::GetIO().DisplaySize = ImVec2(100.0f, 100.0f);
            _runner.set_backend(&_backend);
            _runner.set_renderer(&_renderer);
            _runner.set_build_frame([this] { return ++_frames < 3; });
            ASSERT_TRUE(_runner.run());
        }

        FakeBackend _backend;
        FakeRenderer _renderer;
        ImguiRunner _runner {"imgui-runner"};
        int _frames = 0;
};

TEST_F(TestImguiRunner, test_runner_stops_when_the_gui_closes)
{
    ASSERT_NO_FATAL_FAILURE(run_until_the_gui_closes());

    EXPECT_EQ(_frames, 3);
    EXPECT_FALSE(_runner.is_running());
}

TEST_F(TestImguiRunner, test_runner_refuses_to_run_again_without_imgui)
{
    ASSERT_NO_FATAL_FAILURE(run_until_the_gui_closes());

    EXPECT_FALSE(_runner.run());
    EXPECT_EQ(_frames, 3);
}

} // namespace test
