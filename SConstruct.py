from genericpath import exists
import platform
# Time
import time
build_start_time = time.time()

# General utilities
import sys
from os import getenv
from os.path import join, abspath, isdir, isfile, relpath
# Utils for copying resources to build
from glob import glob
from distutils.dir_util import copy_tree
import fileinput
from subprocess import call as subprocess_call
# Pretty utility for verbosis
from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

# Loading app configuration and build tools, no bytecode for this
sys.dont_write_bytecode = True
import app
import _build_tools.modules
sys.dont_write_bytecode = False

#
# Args
#

verbose = getenv("verbose", "") == "1"
modules_to_build = getenv('modules', "")
distribution = getenv('dist', "") == "1"
make_tests = getenv('test', "") == "1"
build_platform = getenv('platform', "")
clang = getenv('clang', "") == "1"

# Specific
conditionnals = []
if getenv('lua', "") == "1":
    conditionnals.extend(["lua", "luabin"])
if getenv('py', "") == "1":
    conditionnals.append("py")

if build_platform == "windows" and platform.system() == "Linux":
    app.libs.remove('dl')

try:
    build_modules = _build_tools.modules.build_modules(app,
        specific_modules=modules_to_build,
        conditionnals=conditionnals)
    extlibs = _build_tools.modules.build_libs(app, test=make_tests)
    extheaders = _build_tools.modules.build_headers(app, test=make_tests)
except RuntimeError as e:
    print("scons build error: " + str(e), file=sys.stderr)
    Exit(1)

if verbose:
    print("{}: modules configuration".format(app.name))
    pp.pprint(build_modules)
    print("{}: external libs".format(app.name))
    pp.pprint(extlibs)
    print("{}: external headers".format(app.name))
    pp.pprint(extheaders)
    print()

#
# Main shared environment
#

base_env = Environment(
    ENV = {
        'PATH': getenv("PATH"),
    },
    CXX = "c++",
    CCFLAGS = ['-Wall', '-Wextra', '-Werror'] + (hasattr(app, 'flags') and app.flags or []),
    CPPFLAGS = ["-std=c++17"],
    CPPDEFINES = [] + (hasattr(app, 'defines') and app.defines or []),
    CPPPATH = [],
    LIBS = [],
    APP_MODULES_BUILD = build_modules.keys(),
)

if clang:
    base_env["CXX"] = "clang++"
    base_env["CPPFLAGS"].append(["-stdlib=libc++", '-fcxx-exceptions'])
    #base_env["LINKFLAGS"].append(['-undefined dynamic_lookup'])
    base_env.ParseConfig("llvm-config --libs --ldflags --system-libs")

if build_platform.lower() == "windows" and platform.system() == "Linux":
    base_env.Append(tools = ['mingw'])
    base_env["CXX"] = "x86_64-w64-mingw32-g++"
    base_env["CPPDEFINES"].append("_WIN64")

if not verbose:
    base_env["SHCXXCOMSTR"] = "Compiling shared C++: $SOURCE"
    base_env["SHLINKCOMSTR"] = "Linking shared library: $TARGET"
    base_env["CXXCOMSTR"] = "Compiling C++: $SOURCE"
    base_env["LINKCOMSTR"] = "Linking object files into executable: $TARGET"

# Path for build directories
build_dir = Dir('build')
base_env["APP_BUILD"] = build_dir
for entry in ['bin', 'lib', 'include', 'obj', 'test', 'etc']:
    entry_dir = build_dir.Dir(entry)
    base_env["APP_BUILD_" + entry.upper()] = entry_dir

# Path for extlibs bin, lib and include directories
extlib_dir = build_dir.Dir("extlib")
base_env["APP_EXTLIB"] = extlib_dir
for entry in ['bin', 'lib', 'include']:
    entry_dir = extlib_dir.Dir(entry)
    base_env["APP_EXTLIB_" + entry.upper()] = entry_dir
extlib_include_path = str(base_env["APP_EXTLIB_INCLUDE"])

# Setting those path for the compiler
base_env["LIBPATH"] = [base_env["APP_BUILD_LIB"], base_env["APP_EXTLIB_LIB"]]
base_env["CPPPATH"] = [base_env["APP_EXTLIB_INCLUDE"]]

if distribution:
    base_env.Append(RPATH = [
        relpath(str(base_env["APP_BUILD_LIB"]), str(base_env["APP_BUILD_BIN"])),
        relpath(str(base_env["APP_EXTLIB_LIB"]), str(base_env["APP_BUILD_BIN"])),
    ])
else:
    base_env.Append(RPATH = [
        abspath(str(base_env["APP_BUILD_LIB"])),
        abspath(str(base_env["APP_EXTLIB_LIB"]))
    ])

Decider('timestamp-newer')

#
# Build
#

targets = []
def add_targets(src):
    """ Remember every source file used to build """
    global targets
    if isinstance(src, str):
        targets.append(File(src))
    else:
        targets += src

def build_test(self, src=None, libs=[], test_name=None):
    """ Environment method to build unit test binary for a module """
    if make_tests == False:
        return None
    src = src or Glob('test/*.cpp')
    # Not only main.cpp
    add_targets(src)
    if test_name is None:
        test_name = self["APP_MODULE"]
    test_path = join("$APP_BUILD_TEST", "bin", test_name)
    env = self.Clone()
    if not libs:
        libs = [self['APP_MODULE']]
    env.Append(LIBS = libs)
    return env.Program(test_path, src)

def build_lib(self, src=None, lib_name=None):
    """ Environment method to build a shared library for a module """
    src = src or Glob('src/*.cpp')
    add_targets(src)
    module_name = self["APP_MODULE"]
    if lib_name is None:
        lib_name = module_name
    lib_path = join("$APP_BUILD_LIB", lib_name)
    lib = self.SharedLibrary(lib_path, src)
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
    return ["{}_{}".format(app.name, m) for m in args]

def load_env_packages_config(env, *configs):
    return load_env_packages_specific_config(env, [
        "pkg-config {} --cflags --libs".format(config)
        for config in configs
    ])

def load_env_packages_specific_config(env, *configs):
    for config in configs:
        try:
            env.ParseConfig(config)
        except OSError as e:
            print("scons: build warning: package {} not found".format(config), file=sys.stderr)

built = {}
build_obj_path = str(base_env["APP_BUILD_OBJ"])
build_path = str(base_env["APP_BUILD"])
build_etc_path = str(base_env["APP_BUILD_ETC"])

for name, conf in build_modules.items():
    print("scons: building {}'s module: {}".format(app.name, name))
    module_format = "{}_{}".format(app.name, name)
    # Getting module's configurations
    depends = conf.get("depends", [])
    libs = conf.get("libs", [])
    headers = conf.get("headers", [])
    flags = conf.get("flags", "")
    # Create an environment for every module
    env = base_env.Clone()
    env.Append(
        CPPPATH = get_modules_headers(name, *depends) + headers,
        LIBS = get_modules_libname(*depends) + libs + extlibs,
        CCFLAGS = flags,
        APP_MODULE = module_format,
        APP_MODULE_CONF = conf,
    )
    env.AddMethod(build_lib, "build_lib")
    env.AddMethod(build_bin, "build_bin")
    env.AddMethod(build_test, "build_test")
    package_configs = conf.get("pkg-configs", [])
    load_env_packages_config(env, *package_configs)
    parse_configs = conf.get("parse-configs", [])
    load_env_packages_specific_config(env, *parse_configs)
    if verbose:
        print("- needed libraries")
        pp.pprint([ str(s) for s in env['LIBS'] ])
        print("- needed headers")
        pp.pprint([ str(s) for s in env['CPPPATH'] ])
        if package_configs:
            print("- needed packages configs")
            pp.pprint(package_configs)
        if parse_configs:
            print("- needed specific packages configs")
            pp.pprint(parse_configs)
        print()
    # read module's scons script file
    module_dir = Dir(name)
    built[name] = SConscript(module_dir.File("scons.py"),
                            variant_dir = join(build_obj_path, name),
                            duplicate = 0,
                            exports = ['env'])
    # copy module/etc content to build/etc
    etc_dir = str(module_dir.Dir("etc"))
    if isdir(etc_dir):
        if verbose:
            print("Copying resources of module: " + name)
        copy_tree(etc_dir, build_etc_path)

#
# Extra
#

def sed_replace(file, replace_dic):
    for key, value in replace_dic.items():
        subprocess_call(['sed', '-i', 's/{}/{}/g'.format(key, value), file])

def fileinput_replace(file, replace_dic):
    for line in fileinput.input(file, inplace=True):
        for key, value in replace_dic.items():
            print(line.replace(key, value), end='')

def replace_res_in_build(to_replace, replace_dic):
    true_replace = []
    for pattern in to_replace:
        ret = glob(join(build_path, pattern), recursive = True)
        if ret:
            true_replace += ret
        else:
            true_replace.append(pattern)
    if verbose:
        print("Replacing values in build files - {} files to replace".format(len(true_replace)))
    for file in true_replace:
        if isfile(file) == False:
            print("-> file to replace '{}' does not exists".format(file))
            continue
        if verbose:
            print("-> replacing file: " + file)
        sed_replace(file, replace_dic)

if hasattr(app, "replace_files") and hasattr(app, "replace_vars"):
    replace_res_in_build(app.replace_files, app.replace_vars)

#
# Progress
#

try:
    screen = open('/dev/tty', 'w')
    node_count = 0
    node_count_max = len(targets)
    node_count_interval = 1
    node_count_fname = str(base_env.Dir('#')) + '/.scons_node_count'

    def progress_function(node):
        if node not in targets:
            return
        global node_count
        node_count += 1
        if node_count_max > 0 :
            screen.write('\r[%3d%%] ' % (node_count * 100 / node_count_max))

    Progress(progress_function, interval = 1)
except (OSError, IOError) as e:
    print("scons build error: won't display progress - reason: " + str(e), file=sys.stderr)

#
# Status
#

import atexit

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
        print("==============================================================", file=sys.stderr)
        print("scons: BUILD FAILED (took {:.3f} sec)".format(time.time() - build_start_time), file=sys.stderr)
        print(failures_message, file=sys.stderr)
        print("==============================================================", file=sys.stderr)
        if hasattr(app, "on_build_fail"):
            app.on_build_fail(build_modules.keys())
    elif status == 'ok':
        print("scons: build succeeded (took {:.3f} sec)".format(time.time() - build_start_time))
        if hasattr(app, "on_build_success"):
            app.on_build_success(build_modules.keys(), str(build_dir))

atexit.register(display_build_status)