from os.path import join, abspath

projname = "sihd"

base_extlib_hdr = []
base_extlib_lib = ['gtest', 'pthread']
modules = {
    "core": {
    },
    "net": {
        "depends": ['core'],
        "libs": ['websockets']
    },
    "lua": {
        "depends": ['core']
    },
    "luabin": {
        "depends": ['lua']
    }
}

#
# Env
#

globalenv = Environment(
    CC = "c++",
    CCFLAGS = '-Wall -Wextra -Werror',
    CPPFLAGS = "-std=c++0x -pthread",
    CPPDEFINES = [],
    CPPPATH = [],
    LIBS = [],
)
if ARGUMENTS.get("verbose", "") != "1":
    globalenv["SHCXXCOMSTR"] = "Compiling shared C++: $SOURCE"
    globalenv["SHLINKCOMSTR"] = "Linking shared library: $TARGET"
    globalenv["CXXCOMSTR"] = "Compiling C++: $SOURCE"
    globalenv["LINKCOMSTR"] = "Linking object files into executable: $TARGET"

build_dir = Dir('build')
globalenv["SIHD_BUILD"] = build_dir
for entry in ['bin', 'lib', 'include', 'obj', 'test']:
    entry_dir = build_dir.Dir(entry)
    globalenv["SIHD_BUILD_" + entry.upper()] = entry_dir

extlib_dir = build_dir.Dir("extlib")
globalenv["SIHD_EXTLIB"] = extlib_dir
for entry in ['bin', 'lib', 'include']:
    entry_dir = extlib_dir.Dir(entry)
    globalenv["SIHD_EXTLIB_" + entry.upper()] = entry_dir
extlib_include_path = str(globalenv["SIHD_EXTLIB_INCLUDE"])

globalenv["LIBPATH"] = [globalenv["SIHD_BUILD_LIB"], globalenv["SIHD_EXTLIB_LIB"]]
globalenv["CPPPATH"] = [globalenv["SIHD_EXTLIB_INCLUDE"]]
globalenv["RPATH"] = [
    abspath(str(globalenv["SIHD_BUILD_LIB"])),
    abspath(str(globalenv["SIHD_EXTLIB_LIB"]))
]
#globalenv.ParseConfig("pkg-config x11 --cflags --libs")

#
# Args
#

def rec_get_single_module_dependencies(module_name, modlist: dict):
    conf = modules[module_name]
    modlist[module_name] = conf
    for dep in conf.get('depends', []):
        rec_get_single_module_dependencies(dep, modlist)

def rec_get_module_real_depends(module_name, modlist: dict):
    conf = modules[module_name]
    depends = conf.get('depends', [])
    for depend in depends:
        modlist[depend] = True
        rec_get_module_real_depends(depend, modlist)

def fill_module_real_depends(modules: dict):
    for name, conf in modules.items():
        depends = {}
        rec_get_module_real_depends(name, depends)
        conf['depends'] = list(depends.keys())

single_module = ARGUMENTS.get('module', None)
if single_module:
    build_modules = {}
    rec_get_single_module_dependencies(single_module, build_modules)
else:
    build_modules = modules

fill_module_real_depends(build_modules)

Decider('timestamp-newer')

#
# Build
#

targets = []

def add_targets(src):
    global targets
    if isinstance(src, str):
        targets.append(File(src))
    else:
        targets += src

def build_test(self, src=None):
    src = src or Glob('test/*.cpp')
    add_targets(src)
    test_path = join("$SIHD_BUILD_TEST", self["SIHD_MODULE"])
    return self.Program(test_path, src)

def build_lib(self, src=None):
    src = src or Glob('src/*.cpp')
    add_targets(src)
    module_name = self["SIHD_MODULE"]
    lib_path = join("$SIHD_BUILD_LIB", module_name)
    lib = self.SharedLibrary(lib_path, src)
    self.Append(LIBS = [module_name])
    return lib

def build_bin(self, src):
    add_targets(src)
    bin_path = join("$SIHD_BUILD_BIN", self["SIHD_MODULE"])
    return env.Program(bin_path, src)

def get_modules_headers(*args):
    return [join("#", m, "include") for m in args]

def get_modules_libname(*args):
    return ["{}_{}".format(projname, m) for m in args]

def get_extlib_headers(*args):
    ret = []
    for m in args:
        ret += Glob(join(extlib_include_path, "*{}*".format(m)))
    return ret

built = {}
build_obj_path = str(globalenv["SIHD_BUILD_OBJ"])
for name, conf in build_modules.items():
    module_format = "{}_{}".format(projname, name)
    env = globalenv.Clone()
    depends = conf.get("depends", [])
    libs = conf.get("libs", [])
    env.Append(
        CPPPATH = get_modules_headers(name)
                    + get_modules_headers(*depends)
                    + get_extlib_headers(*libs, *base_extlib_lib, *base_extlib_hdr),
        LIBS = get_modules_libname(*depends)
                    + libs + base_extlib_lib,
    )
    env["SIHD_MODULE"] = module_format
    env["SIHD_MODULE_NAME"] = name
    env["SIHD_MODULE_DEPENDS"] = depends
    env["SIHD_MODULE_LIBS"] = depends
    env.AddMethod(build_lib, "build_lib")
    env.AddMethod(build_bin, "build_bin")
    env.AddMethod(build_test, "build_test")

    print("Building {} module: {}".format(projname, name))

    built[name] = SConscript(Dir(name).File("scons.py"),
                            variant_dir = join(build_obj_path, name),
                            duplicate = 0,
                            exports = ['env'])

#
# Progress
#

screen = open('/dev/tty', 'w')
node_count = 0
node_count_max = len(targets)
node_count_interval = 1
node_count_fname = str(env.Dir('#')) + '/.scons_node_count'

def progress_function(node):
    if node not in targets:
        return
    global node_count
    node_count += 1
    if node_count_max > 0 :
        screen.write('\r[%3d%%] ' % (node_count * 100 / node_count_max))

Progress(progress_function, interval = 1)