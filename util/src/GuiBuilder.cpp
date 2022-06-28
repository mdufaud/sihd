#include <sihd/util/GuiBuilder.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

SIHD_LOGGER

GuiBuilder::GuiBuilder()
{
}

GuiBuilder::GuiBuilder(const Block & win)
{
    this->set_window_size(win);
}

GuiBuilder::~GuiBuilder()
{
}

void    GuiBuilder::clear_subwindows()
{
    _subwindow_conf.clear();
}

void    GuiBuilder::set_window_size(const Block & win)
{
    _win = win;
}

int GuiBuilder::get_x_from_gridsize(int gridsize) const
{
    gridsize = std::max(gridsize, 0);
    gridsize = std::min(gridsize, GuiBuilder::grid_max_x);
    return _win.max_x * (gridsize / (float)GuiBuilder::grid_max_x);
}

int GuiBuilder::get_y_from_gridsize(int gridsize) const
{
    gridsize = std::max(gridsize, 0);
    gridsize = std::min(gridsize, GuiBuilder::grid_max_y);
    return _win.max_y * (gridsize / (float)GuiBuilder::grid_max_y);
}

std::string     GuiBuilder::conf_str(const std::vector<Block> & blocks)
{
    std::string ret;
    int last_x = std::numeric_limits<int>::max();
    int last_y = std::numeric_limits<int>::min();
    int line_count = 0;

    size_t i = 0;
    while (i < blocks.size())
    {
        const Block & block = blocks.at(i);
        if (last_x > block.x && last_y < block.y)
        {
            ret += fmt::format("Line[{}] y={}\n", line_count, block.y);
            ++line_count;
            last_x = block.x;
            last_y = block.y;
        }
        ret += fmt::format("\tBlock[{}] y={} x={} max_y={} max_x={}\n", i, block.y, block.x, block.max_y, block.max_x);
        ++i;
    }
    return ret;
}

void    GuiBuilder::add_subwindow(const GuiConf & conf)
{
    _subwindow_conf.push_back(conf);
}

void    GuiBuilder::add_subwindows(const std::vector<GuiConf> & conf)
{
    _subwindow_conf.insert(_subwindow_conf.end(), conf.begin(), conf.end());
}


std::array<int, 4>  GuiBuilder::grid_pos(const GuiConf & conf) const
{
    int starting_y = this->get_y_from_gridsize(conf.grid_push.top);
    starting_y += conf.margin.top;

    int starting_x = this->get_x_from_gridsize(conf.grid_push.left);
    starting_x += conf.margin.left;

    int wanted_y = this->get_y_from_gridsize(conf.grid_y);
    wanted_y -= (starting_y + conf.margin.bottom);

    if (conf.max_y >= 0)
        wanted_y = std::min(wanted_y, conf.max_y);
    if (conf.min_y >= 0)
        wanted_y = std::max(wanted_y, conf.min_y);


    int wanted_x = this->get_x_from_gridsize(conf.grid_x);
    wanted_x -= conf.margin.right;

    if (conf.max_x >= 0)
        wanted_x = std::min(wanted_x, conf.max_x);
    if (conf.min_x >= 0)
        wanted_x = std::max(wanted_x, conf.min_x);

    return {wanted_y, wanted_x, starting_y, starting_x};
}

std::vector<GuiBuilder::Block>  GuiBuilder::build_grid() const
{
    int current_max_y = 0;
    int current_y = _win.y;
    int last_grid_right = 0;
    std::vector<Block> ret;

    for (const auto & conf: _subwindow_conf)
    {
        if (conf.hidden)
            continue ;

        const auto [wanted_y, wanted_x, starting_y, starting_x] = grid_pos(conf);

        if (ret.empty())
        {
            // first block
            Block & new_block = ret.emplace_back(Block {
                .y = current_y + starting_y,
                .x = _win.x + starting_x,
                .max_y = wanted_y,
                .max_x = wanted_x,
            });
            current_max_y = new_block.max_y;
        }
        else
        {
            Block & last_block = ret.back();
            int remaining_x = (_win.max_x - _win.x) - (last_block.x + last_block.max_x);
            if (last_grid_right)
            {
                remaining_x -= this->get_x_from_gridsize(last_grid_right);
            }
            if ((starting_x + wanted_x) > remaining_x)
            {
                current_y += current_max_y;
                // need a new line
                 Block & new_block = ret.emplace_back(Block {
                    .y = current_y + starting_y,
                    .x = _win.x + starting_x,
                    .max_y = wanted_y,
                    .max_x = wanted_x,
                });
                current_max_y = new_block.max_y;
            }
            else
            {
                // can fit into line
                current_max_y = std::max(current_max_y, wanted_y);
                ret.emplace_back(Block {
                    .y = current_y + starting_y,
                    .x = last_block.x + last_block.max_x + starting_x,
                    .max_y = wanted_y,
                    .max_x = wanted_x,
                });
            }
        }
        // TODO grid bottom ?
        last_grid_right = conf.grid_push.right;
    }
    return ret;
}

GuiBuilder::Directions GuiBuilder::Directions::all(int val)
{
    return Directions {
        .left = val,
        .right = val,
        .top = val,
        .bottom = val,
    };
}

GuiBuilder::Directions GuiBuilder::Directions::both(int left_right, int top_bottom)
{
    return Directions {
        .left = left_right,
        .right = left_right,
        .top = top_bottom,
        .bottom = top_bottom,
    };
}

}