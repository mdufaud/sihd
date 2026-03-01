Import('env')

builder = env.builder()
conf = env.module_conf()

srcs = Glob('src/*.cpp')

lib = env.build_lib(srcs)

sihd_util_libname = env.module_format_name()

env.build_demo("demo/util_demo.cpp", name = "util_demo", add_libs = [sihd_util_libname])
env.build_demo("demo/scheduler_bench.cpp", name = "scheduler_bench", add_libs = [sihd_util_libname])

test = env.build_test(Glob('test/*.cpp'), add_libs = [sihd_util_libname])