#ifndef __SIHD_UTIL_GRIDBUILDER_HPP__
# define __SIHD_UTIL_GRIDBUILDER_HPP__

# include <vector>
# include <array>
# include <string>

namespace sihd::util
{

class GridBuilder
{
    public:
        inline static const int grid_max_y = 12;
        inline static const int grid_max_x = 12;

        struct Directions
        {
            Directions() {}
            Directions(int all):
                left(all), right(all), top(all), bottom(all) {}
            Directions(int left_right, int top_bottom):
                left(left_right), right(left_right), top(top_bottom), bottom(top_bottom) {}
            Directions(int left, int right, int top, int bottom):
                left(left), right(right), top(top), bottom(bottom) {}

            int left = 0;
            int right = 0;
            int top = 0;
            int bottom = 0;
        };

        struct GuiConf
        {
            int grid_y = -1;
            int min_y = -1;
            int max_y = -1;

            int grid_x = -1;
            int min_x = -1;
            int max_x = -1;

            Directions grid_push = {0};
            Directions margin = {0};
            Directions padding = {0};

            bool visible = true;
            bool hidden = false;
        };

        struct Block
        {
            int y = 0;
            int x = 0;
            int max_y = 0;
            int max_x = 0;
        };

        GridBuilder();
        GridBuilder(const Block & win);
        virtual ~GridBuilder();

        void set_window_size(const Block & win);
        int get_y_from_gridsize(int gridsize) const;
        int get_x_from_gridsize(int gridsize) const;
        std::array<int, 4> grid_pos(const GuiConf & conf) const;

        void add_subwindow(const GuiConf & conf);
        void add_subwindows(const std::vector<GuiConf> & conf);
        void clear_subwindows();
        std::vector<Block> build_grid() const;

        static std::string conf_str(const std::vector<Block> & blocks);

    protected:

    private:
        Block _win;
        std::vector<GuiConf> _subwindow_conf;
};

}

#endif