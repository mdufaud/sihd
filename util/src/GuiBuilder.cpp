#include <sihd/util/GuiBuilder.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

SIHD_LOGGER


GuiBuilder::GuiBuilder()
{
}

GuiBuilder::GuiBuilder(int max_y, int max_x)
{
    this->set_window_size(max_y, max_x);
}

GuiBuilder::~GuiBuilder()
{
}

void    GuiBuilder::set_window_size(int max_y, int max_x)
{
    _max_y = max_y;
    _max_x = max_x;
}

int GuiBuilder::_get_blocksize_x(int blocksize, int max_x)
{
    return max_x * (blocksize / (float)GuiBuilder::max_blocksize_x);
}

int GuiBuilder::_get_blocksize_y(int blocksize, int max_y)
{
    return max_y * (blocksize / (float)GuiBuilder::max_blocksize_y);
}

std::string     GuiBuilder::dump() const
{
    std::stringstream ss;
    for (auto i = 0u; i < _block_lines.size(); ++i)
    {
        const BlockLine & blockline = _block_lines.at(i);
        ss << "Line[" << i << "]:\n";
        for (auto j = 0u; j < blockline.blocks.size(); ++j)
        {
            const Block & block = blockline.blocks.at(j);
            ss << "\tBlock[" << j << "]:"
                " y: " << block.y
                << " x: " << block.x
                << " max_y: " << block.max_y
                << " max_x: " << block.max_x
                << "\n";
        }
    }
    return ss.str();
}

void    GuiBuilder::clear()
{
    _block_lines.clear();
}

const GuiBuilder::Block &   GuiBuilder::new_child(const GuiConf & conf)
{
    int wanted_y = GuiBuilder::_get_blocksize_y(conf.blocksize_y, _max_y);
    int wanted_x = GuiBuilder::_get_blocksize_x(conf.blocksize_x, _max_x);

    if (_block_lines.empty())
    {
        // first block
        BlockLine & new_block_line = _block_lines.emplace_back(BlockLine {
            .begin_y = 0,
            .max_lines = wanted_y,
            .blocks = {}
        });
        new_block_line.blocks.emplace_back(Block {
            .y = 0,
            .x = 0,
            .max_y = wanted_y,
            .max_x = wanted_x,
        });
    }
    else
    {
        BlockLine & current_block_line = _block_lines.back();
        Block & last_block = current_block_line.blocks.back();
        int remaining_x = _max_x - last_block.max_x;
        if (wanted_x > remaining_x)
        {
            // need a new line
            BlockLine & new_block_line = _block_lines.emplace_back(BlockLine {
                .begin_y = current_block_line.begin_y + current_block_line.max_lines,
                .max_lines = wanted_y,
                .blocks = {}
            });
            new_block_line.blocks.emplace_back(Block {
                .y = new_block_line.begin_y,
                .x = 0,
                .max_y = wanted_y,
                .max_x = wanted_x,
            });
        }
        else
        {
            // can fit into line
            current_block_line.max_lines = std::max(current_block_line.max_lines, wanted_y);
            current_block_line.blocks.emplace_back(Block {
                .y = current_block_line.begin_y,
                .x = last_block.x + last_block.max_x,
                .max_y = wanted_y,
                .max_x = wanted_x,
            });
        }
    }
    return _block_lines.back().blocks.back();
}


}