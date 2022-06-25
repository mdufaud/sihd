#ifndef __SIHD_UTIL_GUIBUILDER_HPP__
# define __SIHD_UTIL_GUIBUILDER_HPP__

# include <vector>
# include <string>

namespace sihd::util
{

class GuiBuilder
{
    public:
        inline static const int max_blocksize_y = 12;
        inline static const int max_blocksize_x = 12;

        struct GuiConf
        {
            int blocksize_y = -1;
            int min_y = -1;
            int max_y = -1;
            int blocksize_x = -1;
            int min_x = -1;
            int max_x = -1;
        };

        struct Block
        {
            int y;
            int x;
            int max_y;
            int max_x;
        };

        GuiBuilder();
        GuiBuilder(int max_y, int max_x);
        virtual ~GuiBuilder();

        void set_window_size(int max_y, int max_x);

        const Block & new_child(const GuiConf & conf);
        void clear();

        std::string dump() const;

    protected:
        static int  _get_blocksize_y(int blocksize, int max_y);
        static int  _get_blocksize_x(int blocksize, int max_x);

    private:
        struct BlockLine
        {
            int begin_y;
            int max_lines;
            std::vector<Block> blocks;
        };

        int _max_x;
        int _max_y;

        std::vector<BlockLine> _block_lines;
};

}

#endif