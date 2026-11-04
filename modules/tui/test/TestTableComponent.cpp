#include <string>
#include <vector>

#include <ftxui/component/event.hpp>
#include <gtest/gtest.h>

#include <sihd/tui/TableComponent.hpp>
#include <sihd/util/Logger.hpp>

#include "render_helper.hpp"

namespace test
{

using namespace sihd::util;
using namespace sihd::tui;
using namespace ftxui;

class TestTableComponent: public ::testing::Test
{
    protected:
        TestTableComponent() { LoggerManager::stream(); }

        virtual ~TestTableComponent() { LoggerManager::clear_loggers(); }

        Component make_table()
        {
            TableOptions options;
            options.columns = {"name", "dbm"};
            options.rows = [this] {
                return _rows;
            };
            options.sort_column = 1;
            return TableComponent(std::move(options));
        }

        TableRows _rows = {{"home", "-40"}, {"cafe", "-72"}, {"lab", "-9"}};
};

TEST_F(TestTableComponent, test_table_sorts_numbers_as_numbers)
{
    Component table = this->make_table();

    EXPECT_EQ(
        render_lines(table->Render(), 20, 5),
        (std::vector<std::string> {"name   dbm ^", "────────────────────", "cafe   -72", "home   -40", "lab    -9"}));
}

TEST_F(TestTableComponent, test_table_keys_change_the_sort)
{
    Component table = this->make_table();

    EXPECT_TRUE(table->OnEvent(Event::Character('r')));
    EXPECT_EQ(render_lines(table->Render(), 20, 5)[2], "lab    -9");

    EXPECT_TRUE(table->OnEvent(Event::ArrowLeft));
    const std::vector<std::string> lines = render_lines(table->Render(), 20, 5);
    EXPECT_EQ(lines[0], "name v dbm");
    EXPECT_EQ(lines[2], "lab    -9");
    EXPECT_EQ(lines[4], "cafe   -72");
}

TEST_F(TestTableComponent, test_table_selection_stays_in_the_rows)
{
    Component table = this->make_table();
    table->Render();

    EXPECT_TRUE(table->OnEvent(Event::End));
    EXPECT_TRUE(table->OnEvent(Event::ArrowDown));
    EXPECT_TRUE(table->OnEvent(Event::Home));
    EXPECT_TRUE(table->OnEvent(Event::ArrowUp));
    EXPECT_FALSE(table->OnEvent(Event::Character('x')));

    _rows.clear();
    EXPECT_EQ(render_lines(table->Render(), 20, 3),
              (std::vector<std::string> {"name   dbm ^", "────────────────────", ""}));
}

} // namespace test
