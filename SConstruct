from os.path import join, abspath

app_name = "sihd"

base_extlib_hdr = []
base_extlib_lib = ['gtest', 'pthread']
modules = {
    "core": {},
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
# Main shared environment
#

globalenv = Environment(
    CC = "c++",
    CCFLAGS = '-Wall -Wextra -Werror',
    CPPFLAGS = ["-std=c++20"],
    CPPDEFINES = [],
    CPPPATH = [],
    LIBS = [],
)
verbose = ARGUMENTS.get("verbose", "") == "1"
if verbose == False:
    globalenv["SHCXXCOMSTR"] = "Compiling shared C++: $SOURCE"
    globalenv["SHLINKCOMSTR"] = "Linking shared library: $TARGET"
    globalenv["CXXCOMSTR"] = "Compiling C++: $SOURCE"
    globalenv["LINKCOMSTR"] = "Linking object files into executable: $TARGET"

build_dir = Dir('build')
globalenv["APP_BUILD"] = build_dir
for entry in ['bin', 'lib', 'include', 'obj', 'test']:
    entry_dir = build_dir.Dir(entry)
    globalenv["APP_BUILD_" + entry.upper()] = entry_dir

extlib_dir = build_dir.Dir("extlib")
globalenv["APP_EXTLIB"] = extlib_dir
for entry in ['bin', 'lib', 'include']:
    entry_dir = extlib_dir.Dir(entry)
    globalenv["APP_EXTLIB_" + entry.upper()] = entry_dir
extlib_include_path = str(globalenv["APP_EXTLIB_INCLUDE"])

globalenv["LIBPATH"] = [globalenv["APP_BUILD_LIB"], globalenv["APP_EXTLIB_LIB"]]
globalenv["CPPPATH"] = [globalenv["APP_EXTLIB_INCLUDE"]]
globalenv["RPATH"] = [
    abspath(str(globalenv["APP_BUILD_LIB"])),
    abspath(str(globalenv["APP_EXTLIB_LIB"]))
]

#globalenv.ParseConfig("pkg-config x11 --cflags --libs")

Decider('timestamp-newer')

#
# Args
#

def rec_get_single_module_dependencies(module_name, modlist: dict):
    """ Gets a single module dependency tree """
    conf = modules.get(module_name, None)
    if conf is None:
        print("No such module: {}".format(module_name))
        Exit(1)
    modlist[module_name] = conf
    for dep in conf.get('depends', []):
        rec_get_single_module_dependencies(dep, modlist)

single_module = ARGUMENTS.get('module', "")
if single_module:
    # If a single module build is asked, build its dependencies
    build_modules = {}
    rec_get_single_module_dependencies(single_module, build_modules)
else:
    build_modules = modules

distribution = ARGUMENTS.get('dist', "") == "1"
make_tests = ARGUMENTS.get('test', "") == "1"

#
# Build
#

def rec_get_module_real_depends(module_name, modlist: dict):
    """ Makes a map [modulename] = trashvalue
        to have a single module real dependencies """
    conf = modules.get(module_name, None)
    if conf is None:
        print("No such module: {}".format(module_name))
        Exit(1)
    depends = conf.get('depends', [])
    for depend in depends:
        modlist[depend] = True
        rec_get_module_real_depends(depend, modlist)

def fill_module_real_depends(modules: dict):
    """ Fill all modules real dependency tree """
    for name, conf in modules.items():
        depends = {}
        rec_get_module_real_depends(name, depends)
        conf['depends'] = list(depends.keys())

fill_module_real_depends(build_modules)

targets = []
def add_targets(src):
    """ Remember every source file used to build """
    global targets
    if isinstance(src, str):
        targets.append(File(src))
    else:
        targets += src

def build_test(self, src=None):
    """ Environment method to build unit test binary for a module """
    if make_tests == False:
        return None
    src = src or Glob('test/*.cpp')
    add_targets(src)
    test_path = join("$APP_BUILD_TEST", self["APP_MODULE"])
    return self.Program(test_path, src)

def build_lib(self, src=None):
    """ Environment method to build a shared library for a module """
    src = src or Glob('src/*.cpp')
    add_targets(src)
    module_name = self["APP_MODULE"]
    lib_path = join("$APP_BUILD_LIB", module_name)
    lib = self.SharedLibrary(lib_path, src)
    self.Append(LIBS = [module_name])
    return lib

def build_bin(self, src):
    """ Environment method to build a binary for a module """
    add_targets(src)
    bin_path = join("$APP_BUILD_BIN", self["APP_MODULE"])
    return env.Program(bin_path, src)

def get_modules_headers(*args):
    """ Returns modules headers path """
    return [join("#", m, "include") for m in args]

def get_modules_libname(*args):
    """ Returns modules lib names """
    return ["{}_{}".format(app_name, m) for m in args]

def get_extlib_headers(*args):
    """ Grab external libs headers path """
    ret = []
    for m in args:
        ret += Glob(join(extlib_include_path, "*{}*".format(m)))
    return ret

built = {}
build_obj_path = str(globalenv["APP_BUILD_OBJ"])
for name, conf in build_modules.items():
    module_format = "{}_{}".format(app_name, name)
    # Create an environment for every module
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
    env["APP_MODULE"] = module_format
    env["APP_MODULE_NAME"] = name
    env["APP_MODULE_DEPENDS"] = depends
    env["APP_MODULE_LIBS"] = depends
    env.AddMethod(build_lib, "build_lib")
    env.AddMethod(build_bin, "build_bin")
    env.AddMethod(build_test, "build_test")
    print("Building {} module: {}".format(app_name, name))
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

#
# Fail
#

import atexit
import sys

def bf_to_str(bf):
    """Convert an element of GetBuildFailures() to a string
    in a useful way."""
    import SCons.Errors
    if bf is None: # unknown targets product None in list
        return '(unknown tgt)'
    elif isinstance(bf, SCons.Errors.StopError):
        return str(bf)
    elif bf.node:
        return str(bf.node) + ': ' + bf.errstr
    elif bf.filename:
        return bf.filename + ': ' + bf.errstr
    return 'unknown failure: ' + bf.errstr

def build_status():
    """Convert the build status to a 2-tuple, (status, msg)."""
    from SCons.Script import GetBuildFailures
    bf = GetBuildFailures()
    if bf:
        # bf is normally a list of build failures; if an element is None,
        # it's because of a target that scons doesn't know anything about.
        status = 'failed'
        failures_message = "\n".join(["Failed building %s" % bf_to_str(x)
                            for x in bf if x is not None])
    else:
        # if bf is None, the build completed successfully.
        status = 'ok'
        failures_message = ''
    return (status, failures_message)

def display_build_status():
    status, failures_message = build_status()
    if status == 'failed':
        print("==============================================================")
        print("= BUILD FAILED =")
        print(failures_message)
        print("==============================================================")
    elif status == 'ok':
        print("Build succeeded.")

atexit.register(display_build_status)