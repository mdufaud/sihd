Import('env')

builder = env.builder()

do_compile = True
if do_compile:
    ftxui_env = env.Clone()
    ftxui_env.Append(
        LIBS = ["pthread"],
        CPPFLAGS = [
            "-Wno-sign-compare",
            "-Wno-pessimizing-move",
            "-Wno-unused-function",
            "-Wno-return-type",
        ],
        CPPDEFINES = [
            "UNICODE",
            "_UNICODE",
            "FTXUI_MICROSOFT_TERMINAL_FALLBACK=ON"
        ],
    )

    conf = env.module_conf()
    cloned = ftxui_env.git_clone(conf["git-url"], conf["git-branch"], "FTXUI")

    ftxui_src_dir = "FTXUI/src/ftxui"

    # change windows header for cross compiling
    if builder.build_platform == "windows":
        mingw_windows_replace = {"<Windows.h>": "<windows.h>"}
        for file in [
            ftxui_src_dir + "/screen/screen.cpp",
            ftxui_src_dir + "/screen/terminal.cpp",
            ftxui_src_dir + "/component/screen_interactive.cpp"
        ]:
            if ftxui_env.find_in_file(file, "<Windows.h>"):
                ftxui_env.file_replace(file, mingw_windows_replace)

    # srcs files for every libs
    ftxui_screen_src_lst = Glob(ftxui_src_dir + "/screen/*.cpp")
    ftxui_dom_src_lst = Glob(ftxui_src_dir + "/dom/*.cpp")
    ftxui_component_src_lst = Glob(ftxui_src_dir + "/component/*.cpp")

    # remove google tests
    ftxui_screen_src_lst = [src for src in ftxui_screen_src_lst if str(src).find("_test") == -1]
    ftxui_dom_src_lst = [src for src in ftxui_dom_src_lst if str(src).find("_test") == -1]
    ftxui_component_src_lst = [src for src in ftxui_component_src_lst if str(src).find("_test") == -1]

    # create environnements for creating the 3 libs
    ft_screen_env = ftxui_env.Clone()
    ft_dom_env = ftxui_env.Clone()
    ft_component_env = ftxui_env.Clone()

    # copy includes to build
    ftxui_env.copy_into_build("FTXUI/include", "include")

    ftxui_env.copy_into_build(ftxui_src_dir + "/screen/util.hpp", "include/ftxui/screen/util.hpp")
    ftxui_env.copy_into_build(ftxui_src_dir + "/screen/string_internal.hpp", "include/ftxui/screen/string_internal.hpp")

    ftxui_env.copy_into_build(ftxui_src_dir + "/dom/box_helper.hpp", "include/ftxui/dom/box_helper.hpp")
    ftxui_env.copy_into_build(ftxui_src_dir + "/dom/flexbox_helper.hpp", "include/ftxui/dom/flexbox_helper.hpp")
    ftxui_env.copy_into_build(ftxui_src_dir + "/dom/node_decorator.hpp", "include/ftxui/dom/node_decorator.hpp")

    ftxui_env.copy_into_build(ftxui_src_dir + "/component/terminal_input_parser.hpp", "include/ftxui/component/terminal_input_parser.hpp")

    # build ftxui-screen
    ft_screen_env.build_lib(ftxui_screen_src_lst, name = "ftxui-screen")

    # build ftxui-dom
    ft_dom_env.Prepend(LIBS = ["ftxui-screen"])
    ft_dom_env.build_lib(ftxui_dom_src_lst, name = "ftxui-dom")

    # build ftxui-component
    ft_component_env.Prepend(LIBS = ["ftxui-screen", "ftxui-dom"])
    ft_component_env.build_lib(ftxui_component_src_lst, name = "ftxui-component")

# load created ftxui libs in env
env.Prepend(LIBS = ["ftxui-component", "ftxui-dom", "ftxui-screen"])

# build library from lib sources - not added to environnement
lib = env.build_lib(Glob('src/*.cpp'), name = env.module_format_name())

env.build_demos(Glob("demo/*.cpp"), add_libs = [env.module_format_name()])

# build unittest from test sources with newly created lib
test = env.build_test(Glob('test/*.cpp'), add_libs = [env.module_format_name()])

# build ftxui examples
env.Append(
    LIBS = ["pthread"],
    CPPFLAGS = [
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
        "-Wno-pessimizing-move",
        "-Wno-unused-function",
    ]
)

import os
for src in Glob('bin/*.cpp'):
    bin_name = os.path.basename(os.path.splitext(str(src))[0])
    env.build_bin(src, name = bin_name)
