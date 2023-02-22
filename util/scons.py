Import('env')

builder = env.builder()
conf = env.module_conf()

srcs = Glob('src/*.cpp')

lib = env.build_lib(srcs)

env.build_demo("demo/util_demo.cpp", name = "util_demo", add_libs = [env.module_format_name()])

test = env.build_test(Glob('test/*.cpp'), add_libs = [env.module_format_name()])
