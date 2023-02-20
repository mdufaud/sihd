Import('env')

builder = env.builder()

lib = env.build_lib(Glob('src/*.cpp'))

demo_ext = ""

if builder.build_compiler == "em":
    env.Append(CPPFLAGS = ["-s", "WASM=1"])
    demo_ext = ".html"

env.build_demo("demo/util_demo.cpp", name = f"util_demo{demo_ext}", add_libs = [env.module_format_name()])

test = env.build_test(Glob('test/*.cpp'), add_libs = [env.module_format_name()])