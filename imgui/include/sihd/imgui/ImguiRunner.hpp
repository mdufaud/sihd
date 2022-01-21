#ifndef __SIHD_IMGUI_IMGUIRUNNER_HPP__
# define __SIHD_IMGUI_IMGUIRUNNER_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::imgui
{

class ImguiRunner:   public sihd::util::Named
{
    public:
        ImguiRunner(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~ImguiRunner();

    protected:

    private:
};

}

#endif