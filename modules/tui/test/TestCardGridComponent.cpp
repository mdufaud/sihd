#include <string>
#include <vector>

#include <ftxui/component/event.hpp>
#include <gtest/gtest.h>

#include <sihd/tui/CardGridComponent.hpp>
#include <sihd/util/Logger.hpp>

#include "render_helper.hpp"

namespace test
{

using namespace sihd::util;
using namespace sihd::tui;
using namespace ftxui;

class TestCardGridComponent: public ::testing::Test
{
    protected:
        TestCardGridComponent() { LoggerManager::stream(); }

        virtual ~TestCardGridComponent() { LoggerManager::clear_loggers(); }

        Component make_grid()
        {
            CardGridOptions options;
            options.cards = [this] {
                std::vector<Card> cards;
                for (const std::string & key : _keys)
                    cards.push_back({key, key, text("body " + key)});
                return cards;
            };
            options.card_width = 10;
            options.selected = &_selected;
            return CardGridComponent(std::move(options));
        }

        std::vector<std::string> render(Component & grid) { return render_lines(grid->Render(), 25, 6); }

        std::vector<std::string> _keys = {"a", "b", "c"};
        std::string _selected;
};

TEST_F(TestCardGridComponent, test_cardgrid_wraps_cards_by_width)
{
    Component grid = this->make_grid();

    EXPECT_EQ(this->render(grid),
              (std::vector<std::string> {"╔ a ═════╗╭ b ─────╮",
                                         "║body a  ║│body b  │",
                                         "╚════════╝╰────────╯",
                                         "╭ c ─────╮",
                                         "│body c  │",
                                         "╰────────╯"}));
    EXPECT_EQ(_selected, "");
}

TEST_F(TestCardGridComponent, test_cardgrid_arrows_move_by_card_and_by_row)
{
    Component grid = this->make_grid();
    this->render(grid);

    EXPECT_TRUE(grid->OnEvent(Event::ArrowRight));
    EXPECT_EQ(_selected, "b");
    EXPECT_TRUE(grid->OnEvent(Event::ArrowDown));
    EXPECT_EQ(_selected, "c");
    EXPECT_TRUE(grid->OnEvent(Event::ArrowUp));
    EXPECT_EQ(_selected, "a");
    EXPECT_TRUE(grid->OnEvent(Event::ArrowLeft));
    EXPECT_EQ(_selected, "a");
    EXPECT_TRUE(grid->OnEvent(Event::End));
    EXPECT_EQ(_selected, "c");
    EXPECT_FALSE(grid->OnEvent(Event::Character('x')));
}

TEST_F(TestCardGridComponent, test_cardgrid_scrolls_to_the_selected_card)
{
    Component grid = this->make_grid();
    _keys = {"a", "b", "c", "d", "e", "f"};
    this->render(grid);

    EXPECT_TRUE(grid->OnEvent(Event::End));

    EXPECT_EQ(this->render(grid)[4], "│body e  │║body f  ║");
}

TEST_F(TestCardGridComponent, test_cardgrid_selection_follows_the_key)
{
    Component grid = this->make_grid();
    _selected = "b";
    _keys = {"b", "a"};

    EXPECT_EQ(this->render(grid)[0], "╔ b ═════╗╭ a ─────╮");

    _keys = {"a", "c"};
    this->render(grid);
    EXPECT_EQ(_selected, "");

    _keys.clear();
    this->render(grid);
    EXPECT_FALSE(grid->OnEvent(Event::ArrowRight));
}

} // namespace test
