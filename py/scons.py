from os.path import join, exists

Import('env')

modules = env.modules_to_build()

test_dir = Dir("test")
src_dir = Dir("src")

modname = env.module_format_name()
srcs = [src_dir.File("PyApi.cpp")]
libs = []
test_srcs = [test_dir.File("main.cpp"), test_dir.File("DirectorySwitcher.cpp")]
test_libs = []
for module in modules:
    modcap = module.capitalize()
    src_file = src_dir.File("Py{}Api.cpp".format(modcap))
    test_file = test_dir.File("TestPy{}Api.cpp".format(modcap))
    if exists(str(src_file)):
        srcs.append(src_file)
    if exists(str(test_file)):
        test_srcs.append(test_file)

lib = env.build_lib(srcs)
test = env.build_test(test_srcs, add_libs = [env.module_format_name()])

Return("lib")