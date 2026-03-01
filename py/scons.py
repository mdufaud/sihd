from os.path import join, exists

Import('env')

builder = env.builder()

# vcpkg installs python headers in a versioned subdirectory (e.g. python3.12/)
# that is not in the default CPPPATH - add it so pybind11 can find Python.h
python_include = join(builder.build_extlib_hdr_path, "python3.12")
if exists(python_include):
    env.AppendUnique(CPPPATH=[python_include])

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