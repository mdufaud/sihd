#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include <ftxui/component/event.hpp>
#include <gtest/gtest.h>

#include <sihd/core/Channel.hpp>
#include <sihd/core/Device.hpp>
#include <sihd/core/TreeMonitor.hpp>
#include <sihd/tui/TreeMonitorComponent.hpp>
#include <sihd/util/Logger.hpp>

#include "render_helper.hpp"

namespace test
{

using namespace sihd::util;
using namespace sihd::core;
using namespace sihd::tui;

class SwitchDevice: public Device
{
    public:
        SwitchDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return _running; }

    protected:
        bool on_init() override
        {
            this->add_channel("level", "int");
            return true;
        }

        bool on_start() override
        {
            _running = true;
            return true;
        }

        bool on_stop() override
        {
            this->remove_child("level");
            _running = false;
            return true;
        }

    private:
        bool _running = false;
};

class TestTreeMonitorComponent: public ::testing::Test
{
    protected:
        TestTreeMonitorComponent(): _refresher([this] { ++_posts; })
        {
            LoggerManager::stream();
            _root.add_child<Node>("sub");
            new Channel("speed", "int", _root.get_child<Node>("sub"));
            _device = _root.add_child<SwitchDevice>("dev");
        }

        virtual ~TestTreeMonitorComponent() { LoggerManager::clear_loggers(); }

        Node _root {"root"};
        SwitchDevice *_device = nullptr;
        TreeMonitor _monitor;
        int _posts = 0;
        Refresher _refresher;
};

TEST_F(TestTreeMonitorComponent, test_tree_follows_the_monitor)
{
    ASSERT_TRUE(_monitor.observe(&_root));
    ftxui::Component tree = TreeMonitorComponent(_monitor, _refresher);

    EXPECT_EQ(render_lines(tree->Render(), 40, 4),
              (std::vector<std::string> {"▾ root", "  ▾ sub", "      speed = 0", "    dev [none]"}));

    Channel *speed = _root.find<Channel>("sub.speed");
    ASSERT_NE(speed, nullptr);
    ASSERT_TRUE(speed->write<int>(0, 42));
    ASSERT_TRUE(_monitor.flush());
    EXPECT_EQ(_posts, 1);

    ASSERT_TRUE(_device->init());
    ASSERT_TRUE(_device->start());
    ASSERT_TRUE(_monitor.flush());

    EXPECT_EQ(render_lines(tree->Render(), 40, 5),
              (std::vector<std::string> {"▾ root",
                                         "  ▾ sub",
                                         "      speed = 42 (writes: 1)",
                                         "  ▾ dev [running]",
                                         "      level = 0"}));
}

TEST_F(TestTreeMonitorComponent, test_tree_built_before_observe)
{
    ftxui::Component tree = TreeMonitorComponent(_monitor, _refresher);
    EXPECT_EQ(render_lines(tree->Render(), 40, 1), (std::vector<std::string> {""}));

    ASSERT_TRUE(_monitor.observe(&_root));
    ASSERT_TRUE(_monitor.flush());

    EXPECT_EQ(render_lines(tree->Render(), 40, 4),
              (std::vector<std::string> {"▾ root", "  ▾ sub", "      speed = 0", "    dev [none]"}));
}

TEST_F(TestTreeMonitorComponent, test_tree_folds)
{
    ASSERT_TRUE(_monitor.observe(&_root));
    ftxui::Component tree = TreeMonitorComponent(_monitor, _refresher);

    EXPECT_TRUE(tree->OnEvent(ftxui::Event::ArrowDown));
    EXPECT_TRUE(tree->OnEvent(ftxui::Event::ArrowLeft));
    EXPECT_EQ(render_lines(tree->Render(), 40, 3), (std::vector<std::string> {"▾ root", "  ▸ sub", "    dev [none]"}));

    EXPECT_TRUE(tree->OnEvent(ftxui::Event::ArrowLeft));
    EXPECT_TRUE(tree->OnEvent(ftxui::Event::ArrowLeft));
    EXPECT_EQ(render_lines(tree->Render(), 40, 1), (std::vector<std::string> {"▸ root"}));

    EXPECT_TRUE(tree->OnEvent(ftxui::Event::ArrowRight));
    EXPECT_EQ(render_lines(tree->Render(), 40, 3), (std::vector<std::string> {"▾ root", "  ▸ sub", "    dev [none]"}));
}

TEST_F(TestTreeMonitorComponent, test_tree_drops_detached_rows)
{
    ASSERT_TRUE(_monitor.observe(&_root));
    ftxui::Component tree = TreeMonitorComponent(_monitor, _refresher);

    ASSERT_TRUE(_device->init());
    ASSERT_TRUE(_device->start());
    ASSERT_TRUE(_device->stop());
    ASSERT_TRUE(_monitor.flush());

    EXPECT_EQ(render_lines(tree->Render(), 40, 4),
              (std::vector<std::string> {"▾ root", "  ▾ sub", "      speed = 0", "    dev [stopped]"}));
}

TEST_F(TestTreeMonitorComponent, test_render_while_writing)
{
    ASSERT_TRUE(_monitor.observe(&_root));
    Channel *speed = _root.find<Channel>("sub.speed");
    ASSERT_NE(speed, nullptr);

    std::atomic<bool> stop = false;
    std::thread writer([&] {
        for (int i = 1; stop == false; ++i)
            EXPECT_TRUE(speed->write<int>(0, i));
    });
    {
        ftxui::Component tree = TreeMonitorComponent(_monitor, _refresher);
        for (int i = 0; i < 50; ++i)
        {
            EXPECT_EQ(render_lines(tree->Render(), 40, 4).size(), 4u);
            EXPECT_TRUE(tree->OnEvent(i % 8 < 4 ? ftxui::Event::ArrowDown : ftxui::Event::ArrowUp));
        }
    }
    stop = true;
    writer.join();
    EXPECT_TRUE(_monitor.flush());
}

} // namespace test
