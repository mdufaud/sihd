from os.path import join, exists

Import('env')

modules = env.modules_to_build()

test_dir = Dir("test")
src_dir = Dir("src")

modname = env.module_format_name()
srcs = [src_dir.File("PyApi.cpp")]
test_srcs = [test_dir.File("main.cpp"), test_dir.File("DirectorySwitcher.cpp")]
for module in modules:
    if module == "py":
        continue
    srcs += Glob(str(src_dir) + "/{}/*.cpp".format(module))
    test_srcs += Glob(str(test_dir) + "/{}/*.cpp".format(module))

lib = env.build_lib(srcs)
test = env.build_test(test_srcs, add_libs = [env.module_format_name()])

Return("lib")