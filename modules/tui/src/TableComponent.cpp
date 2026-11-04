#include <algorithm>
#include <charconv>
#include <optional>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>

#include <sihd/tui/TableComponent.hpp>

namespace sihd::tui
{

using namespace ftxui;

namespace
{

std::optional<double> number_of(const std::string & cell)
{
    double value = 0;
    const char *end = cell.data() + cell.size();
    const auto [ptr, ec] = std::from_chars(cell.data(), end, value);
    if (ec != std::errc() || ptr != end || cell.empty())
        return std::nullopt;
    return value;
}

bool cell_less(const std::string & a, const std::string & b)
{
    const std::optional<double> number_a = number_of(a);
    const std::optional<double> number_b = number_of(b);
    if (number_a && number_b)
        return *number_a < *number_b;
    if (number_a || number_b)
        return number_a.has_value();
    return a < b;
}

class Table: public ComponentBase
{
    public:
        explicit Table(TableOptions && options): _options(std::move(options)), _selected(0) {}

        Element OnRender() override
        {
            TableRows rows = _options.rows ? _options.rows() : TableRows {};
            const size_t columns = _options.columns.size();
            const size_t sorted = std::min(_options.sort_column, columns == 0 ? 0 : columns - 1);
            std::stable_sort(rows.begin(), rows.end(), [&](const auto & a, const auto & b) {
                const std::string & cell_a = sorted < a.size() ? a[sorted] : _empty;
                const std::string & cell_b = sorted < b.size() ? b[sorted] : _empty;
                return _options.sort_descending ? cell_less(cell_b, cell_a) : cell_less(cell_a, cell_b);
            });
            _row_count = rows.size();
            _selected = std::min(_selected, _row_count == 0 ? 0 : _row_count - 1);

            std::vector<int> widths(columns, 0);
            for (size_t col = 0; col < columns; ++col)
                widths[col] = string_width(_options.columns[col]) + 2;
            for (const auto & row : rows)
            {
                for (size_t col = 0; col < columns && col < row.size(); ++col)
                    widths[col] = std::max(widths[col], string_width(row[col]));
            }

            Elements header;
            for (size_t col = 0; col < columns; ++col)
            {
                std::string title = _options.columns[col];
                if (col == sorted)
                    title += _options.sort_descending ? " v" : " ^";
                header.push_back(text(title) | size(WIDTH, EQUAL, widths[col]));
                header.push_back(text(" "));
            }

            Elements lines;
            for (size_t i = 0; i < rows.size(); ++i)
            {
                Elements cells;
                for (size_t col = 0; col < columns; ++col)
                {
                    const std::string & cell = col < rows[i].size() ? rows[i][col] : _empty;
                    cells.push_back(text(cell) | size(WIDTH, EQUAL, widths[col]));
                    cells.push_back(text(" "));
                }
                Element line = hbox(std::move(cells));
                if (i == _selected)
                    line = this->Focused() ? (line | inverted | ftxui::focus) : (line | inverted | ftxui::select);
                lines.push_back(std::move(line));
            }
            return vbox({hbox(std::move(header)) | bold,
                         separator(),
                         vbox(std::move(lines)) | yframe | vscroll_indicator | flex});
        }

        bool OnEvent(Event event) override
        {
            constexpr size_t page = 10;
            const size_t last = _row_count == 0 ? 0 : _row_count - 1;
            if (event == Event::ArrowUp)
                _selected = _selected == 0 ? 0 : _selected - 1;
            else if (event == Event::ArrowDown)
                _selected = std::min(_selected + 1, last);
            else if (event == Event::PageUp)
                _selected = _selected > page ? _selected - page : 0;
            else if (event == Event::PageDown)
                _selected = std::min(_selected + page, last);
            else if (event == Event::Home)
                _selected = 0;
            else if (event == Event::End)
                _selected = last;
            else if (event == Event::ArrowLeft)
                _options.sort_column = _options.sort_column == 0 ? 0 : _options.sort_column - 1;
            else if (event == Event::ArrowRight && _options.columns.empty() == false)
                _options.sort_column = std::min(_options.sort_column + 1, _options.columns.size() - 1);
            else if (event == Event::Character('r'))
                _options.sort_descending = !_options.sort_descending;
            else
                return false;
            return true;
        }

        bool Focusable() const override { return true; }

    private:
        TableOptions _options;
        size_t _selected;
        size_t _row_count = 0;
        const std::string _empty;
};

} // namespace

Component TableComponent(TableOptions && options)
{
    return Make<Table>(std::move(options));
}

} // namespace sihd::tui
