Import('env', 'builder_helper', 'module_format_name')

conf = env["APP_MODULE_CONF"]

ft_env = env.Clone()
ft_env.Append(
    LIBS = ["pthread"],
    CPPFLAGS = [
        "-Wno-sign-compare",
        "-Wno-pessimizing-move",
        "-Wno-unused-function",
    ],
    CPPDEFINES = [
        "UNICODE",
        "_UNICODE",
        "FTXUI_MICROSOFT_TERMINAL_FALLBACK=ON"
    ],
)

cloned = ft_env.git_clone(conf["git-url"], conf["git-branch"], "FTXUI")

src_dir = "FTXUI/src/ftxui"

mingw_windows_replace = {"<Windows.h>": "<windows.h>"}

ft_env.file_replace(src_dir + "/screen/screen.cpp", mingw_windows_replace)
ft_env.file_replace(src_dir + "/screen/terminal.cpp", mingw_windows_replace)
ft_env.file_replace(src_dir + "/component/screen_interactive.cpp", mingw_windows_replace)

screen_src_lst = Glob(src_dir + "/screen/*.cpp")
dom_src_lst = Glob(src_dir + "/dom/*.cpp")
component_src_lst = Glob(src_dir + "/component/*.cpp")

screen_src_lst = [src for src in screen_src_lst if str(src).find("_test") == -1]
dom_src_lst = [src for src in dom_src_lst if str(src).find("_test") == -1]
component_src_lst = [src for src in component_src_lst if str(src).find("_test") == -1]

ft_screen_env = ft_env.Clone()
ft_dom_env = ft_env.Clone()
ft_component_env = ft_env.Clone()

ft_env.copy_into_build("FTXUI/include", "include")

ft_env.copy_into_build(src_dir + "/screen/util.hpp", "include/ftxui/screen/util.hpp")

ft_env.copy_into_build(src_dir + "/dom/box_helper.hpp", "include/ftxui/dom/box_helper.hpp")
ft_env.copy_into_build(src_dir + "/dom/flexbox_helper.hpp", "include/ftxui/dom/flexbox_helper.hpp")
ft_env.copy_into_build(src_dir + "/dom/node_decorator.hpp", "include/ftxui/dom/node_decorator.hpp")

ft_env.copy_into_build(src_dir + "/component/terminal_input_parser.hpp", "include/ftxui/component/terminal_input_parser.hpp")

ft_screen_env.build_lib(screen_src_lst, name = "ftxui-screen")

ft_dom_env.Prepend(LIBS = ["ftxui-screen"])
ft_dom_env.build_lib(dom_src_lst, name = "ftxui-dom")

ft_component_env.Prepend(LIBS = ["ftxui-screen", "ftxui-dom"])
ft_component_env.build_lib(component_src_lst, name = "ftxui-component")

env.Prepend(LIBS = ["ftxui-screen", "ftxui-dom", "ftxui-component"])

# build library from lib sources - not added to environnement
# lib = env.build_lib(Glob('src/*.cpp'), name = module_format_name)

# build unittest from test sources with newly created lib
# test = env.build_test(Glob('test/*.cpp'), add_libs = [module_format_name])

# build binary from bin sources with newly created lib
bin = env.build_bin(Glob('src/*.cpp'))

# Return('lib')
Return('bin')
