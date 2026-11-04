#ifndef __SIHD_TUI_TABLECOMPONENT_HPP__
#define __SIHD_TUI_TABLECOMPONENT_HPP__

#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

namespace sihd::tui
{

using TableRows = std::vector<std::vector<std::string>>;

struct TableOptions
{
        std::vector<std::string> columns;
        std::function<TableRows()> rows;
        size_t sort_column = 0;
        bool sort_descending = false;
};

ftxui::Component TableComponent(TableOptions && options);

} // namespace sihd::tui

#endif
