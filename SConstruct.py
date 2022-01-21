import platform
# Time
import time
build_start_time = time.time()
# General utilities
import sys
import glob
import os
import fileinput
import subprocess
import shutil
# Pretty utility for verbosis
from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

# Loading app configuration and build tools, no bytecode for this
sys.dont_write_bytecode = True
import app
from _build_tools import modules as modules_helper
from _build_tools import builder as builder_helper
sys.dont_write_bytecode = False

builder_helper.info("building {}".format(app.name))
builder_helper.info("building to: {}".format(builder_helper.build_path))
builder_helper.sanitize_app(app)

###############################################################################
# Build settings
###############################################################################

modules_to_build = builder_helper.get_modules()
has_test = builder_helper.has_test()
verbose = builder_helper.has_verbose()
asan = builder_helper.is_address_sanatizer()
build_static_libs = builder_helper.is_static_libs()

compiler = builder_helper.build_compiler
build_platform = builder_helper.build_platform
compile_mode = builder_helper.build_mode
arch = builder_helper.build_architecture

distribution = builder_helper.do_distribution()

if build_platform not in ("windows", "linux"):
    builder_helper.error("platform {} is not supported".format(build_platform))
    exit(1)
if compiler not in ("gcc", "clang", "mingw", "em"):
    builder_helper.error("compiler {} is not supported".format(compiler))
    exit(1)
if compile_mode not in ("debug", "release"):
    builder_helper.error("mode {} unknown".format(compile_mode))
    exit(1)

if verbose:
    builder_helper.info("platform: " + build_platform)
    builder_helper.info("compiler: " + compiler)
    builder_helper.info("arch: " + arch)
    builder_helper.info("mode: " + compile_mode)
    builder_helper.info("tests: " + (has_test and "yes" or "no"))
    builder_helper.info("address sanatizer: " + (asan and "yes" or "no"))

# Get modules configuration for this build
try:
    build_modules = modules_helper.get_modules(app,
        specific_modules=modules_to_build)
except RuntimeError as e:
    builder_helper.error(str(e))
    Exit(1)
global_libs = hasattr(app, "libs") and app.libs or []
if verbose:
    builder_helper.debug("modules configuration:")
    pp.pprint(build_modules)
    builder_helper.debug("libs:")
    pp.pprint(global_libs)
    print()

###############################################################################
# Build compiling options
###############################################################################

def add_env_app_conf(env, key):
    env_to_key = {
        "LIBS": "_libs",
        "CPPFLAGS": "_flags",
        "CPPDEFINES": "_defines",
        "LINKFLAGS": "_link",
    }
    for envkey, to_concat in env_to_key.items():
        concat = key + to_concat
        if hasattr(app, concat):
            #prepends
            env[envkey][:0] = getattr(app, concat)

build_dir = Dir(builder_helper.build_path)
bin_dir = Dir(builder_helper.build_bin_path)
lib_dir = Dir(builder_helper.build_lib_path)
extlib_dir = Dir(builder_helper.build_extlib_path)
extlib_lib_dir = Dir(builder_helper.build_extlib_lib_path)

base_env = Environment(
    # binaries path
    ENV = {
        'PATH': os.getenv("PATH"),
        'LIBPATH': os.getenv("LIBPATH")
    },
    # compiler for c
    CC = "gcc",
    # compiler for c++
    CXX = "c++",
    # static library archiver
    AR = "ar",
    # static library indexer
    RANLIB = "ranlib",
    # c++ compile flags
    CXXFLAGS = ["-std=c++17"],
    # c and c++ compile flags
    CPPFLAGS = [] + (hasattr(app, 'flags') and app.flags or []),
    # extra #define for inside the code
    CPPDEFINES = [] + (hasattr(app, 'defines') and app.defines or []),
    # link flags
    LINKFLAGS = [],
    # headers path
    CPPPATH = [builder_helper.build_hdr_path, builder_helper.build_extlib_hdr_path],
    # libraries path
    LIBPATH = [builder_helper.build_lib_path, builder_helper.build_extlib_lib_path],
    # libraries name
    LIBS = global_libs,
    # extra key for modules to build
    APP_MODULES_BUILD = build_modules.keys(),
    BUILDER_HELPER = builder_helper,
)

# Build output
if not verbose:
    base_env.Replace(
        SHCCCOMSTR = "compiling shared C: $SOURCE",
        SHCXXCOMSTR = "compiling shared C++: $SOURCE",
        SHLINKCOMSTR = "linking shared library: $TARGET",
        CCCOMSTR = "compiling C: $SOURCE",
        CXXCOMSTR = "compiling C++: $SOURCE",
        ARCOMSTR = "generating static lib: $TARGET",
        RANLIBCOMSTR = "indexing static lib: $TARGET",
        LINKCOMSTR = "linking object files into executable: $TARGET",
    )

if not distribution:
    base_env.Append(
        RPATH = [
            os.path.abspath(builder_helper.build_lib_path),
            os.path.abspath(builder_helper.build_extlib_lib_path)
        ],
    )

if compile_mode == "debug":
    add_env_app_conf(base_env, "debug")
elif compile_mode == "release":
    add_env_app_conf(base_env, "release")

# CLANG build
if compiler == "clang":
    base_env.Replace(
        CC = "clang",
        CXX = "clang++",
        AR = "ar",
        RANLIB = "ranlib",
    )
    if asan:
        # Needs to be first
        base_env.Append(
            LIBS = ["asan"],
            CPPFLAGS = ["-fsanitize=address", "-fno-omit-frame-pointer"],
        )
    base_env.ParseConfig("llvm-config --libs --ldflags --system-libs")
    add_env_app_conf(base_env, "clang")
# MINGW build
elif compiler == "mingw":
    if asan:
        builder_helper.error("cannot use address sanitizer with mingw")
        Exit(1)
    base_env.Replace(
        CC = "x86_64-w64-mingw32-gcc",
        CXX = "x86_64-w64-mingw32-g++",
        AR = "x86_64-w64-mingw32-ar",
        RANLIB = "x86_64-w64-mingw32-ranlib",
    )
    if not build_static_libs:
        base_env.Replace(
            SHLIBSUFFIX = ".dll",
            LIBPREFIX = "",
        )
    add_env_app_conf(base_env, "mingw")
# GCC build
elif compiler == "gcc":
    if asan:
        # Needs to be first
        base_env.Append(
            LIBS = ["asan"],
            CPPFLAGS  = ["-fsanitize=address", "-fno-omit-frame-pointer"],
        )
    add_env_app_conf(base_env, "gcc")
# EMSCRIPTEN build
elif compiler == "em":
    base_env.Replace(
        CC = "emcc",
        CXX = "em++",
        AR = "emar",
        RANLIB = "emranlib",
    )
    add_env_app_conf(base_env, "em")

# Decides when to recompile - removing slow md5 in favor of timestamps
Decider('timestamp-newer')

###############################################################################
# Build
###############################################################################

## modules environnement methods

targets = []
def add_targets(src):
    """ Remember every source file used to build """
    global targets
    if isinstance(src, str):
        targets.append(File(src))
    elif isinstance(src, list):
        targets.extend(src)
    else:
        targets.append(src)

def build_test(self, src=None, add_libs=[], test_name=None, **kwargs):
    """ Environment method to build unit test binary for a module """
    if has_test == False:
        return None
    src = src or Glob('test/*.cpp')
    # Not only main.cpp
    add_targets(src)
    if test_name is None:
        test_name = self["APP_MODULE_NAME"]
    test_env = self.Clone()
    if add_libs is not None:
        if not add_libs:
            add_libs = [self['APP_MODULE_NAME']]
        test_env.Prepend(LIBS = add_libs)
    add_env_app_conf(test_env, "test")
    if compiler == "clang":
        add_env_app_conf(test_env, "clang_test")
    elif compiler == "mingw":
        add_env_app_conf(test_env, "mingw_test")
    elif compiler == "gcc":
        add_env_app_conf(test_env, "gcc_test")
    test_path = os.path.join(builder_helper.build_test_path, "bin", test_name)
    return test_env.Program(test_path, src, **kwargs)

def build_lib(self, src=None, lib_name=None, static=None, **kwargs):
    """ Environment method to build a shared library for a module """
    src = src or Glob('src/*.cpp')
    add_targets(src)
    module_name = self["APP_MODULE_NAME"]
    if lib_name is None:
        lib_name = module_name
    lib_path = os.path.join(builder_helper.build_lib_path, lib_name)
    if (static is not None and static) or build_static_libs:
        lib = self.StaticLibrary(lib_path, src, **kwargs)
    else:
        lib = self.SharedLibrary(lib_path, src, **kwargs)
    return lib

def build_bin(self, src, bin_name=None, **kwargs):
    """ Environment method to build a binary for a module """
    add_targets(src)
    if bin_name is None:
        bin_name = self['APP_MODULE_NAME']
        if compiler == "mingw":
            bin_name += ".exe"
    bin_path = os.path.join(builder_helper.build_bin_path, bin_name)
    return self.Program(bin_path, src, **kwargs)

# methods to build either test, lib or executable
base_env.AddMethod(build_lib, "build_lib")
base_env.AddMethod(build_bin, "build_bin")
base_env.AddMethod(build_test, "build_test")

## build utilities

def get_modules_headers(*args):
    """ Returns modules headers path """
    return [os.path.join("#", m, "include") for m in args]

def get_modules_libname(*args):
    """ Returns modules shared library names """
    return ["{}_{}".format(app.name, m) for m in args]

def load_env_packages_config(env, *configs):
    """ Parse multiple pkg-configs: libraries/includes utilities """
    return load_env_packages_specific_config(env, [
        "pkg-config {} --cflags --libs".format(config)
        for config in configs
    ])

def load_env_packages_specific_config(env, *configs):
    """ Parse configs from binaries outputs """
    for config in configs:
        try:
            env.ParseConfig(config)
        except OSError as e:
            builder_helper.warning("package {} not found".format(config))

def copy_module_dir(module_name, dirname_to_copy):
    """ recursive copy of MODNAME/DIRNAME to build/DIRNAME """
    module_dir_input = Dir(module_name).Dir(dirname_to_copy)
    build_dir_output = build_dir.Dir(dirname_to_copy)
    if os.path.isdir(str(module_dir_input)):
        if verbose:
            builder_helper.info("copying resources '{}' of module: {}".format(dirname_to_copy, module_name))
        shutil.copytree(module_dir_input.get_abspath(), build_dir_output.get_abspath(), dirs_exist_ok = True)

## sorting build order by module dependencies size
build_order = list(build_modules.values())
build_order.sort(key = lambda obj: len(obj["depends"]))
# Configure env and call scons.py from every configured modules
built = {}
for conf in build_order:
    modname = conf["modname"]
    builder_helper.info("building module: {}".format(modname))
    module_format = "{}_{}".format(app.name, modname)
    # Getting module's build configuration
    depends = conf.get("depends", [])
    libs = conf.get("libs", [])
    platform_libs = conf.get("{}-libs".format(build_platform), [])
    flags = conf.get("flags", [])
    platform_flags = conf.get("{}-flags".format(build_platform), [])
    link = conf.get("link", [])
    platform_link = conf.get("{}-link".format(build_platform), [])
    # Create a specific environment for the module
    env = base_env.Clone()
    env.Prepend(
        # adding headers of depending modules and self
        CPPPATH = get_modules_headers(modname, *depends),
        # adding libraries
        LIBS = get_modules_libname(*depends) + libs + platform_libs,
        # adding specified flags
        CCFLAGS = flags + platform_flags,
        # adding specified link flags
        LINKFLAGS = link + platform_link,
        # formatted module name PROJNAME_MODULENAME
        APP_MODULE_NAME = module_format,
        # configuration of module for scons.py
        APP_MODULE_CONF = conf,
    )
    # use multiple pkg-config output to add libraries/includes path
    package_configs = conf.get("pkg-configs", [])
    load_env_packages_config(env, *package_configs)
    # use multiple binaries output to add libraries/includes path
    parse_configs = conf.get("parse-configs", [])
    load_env_packages_specific_config(env, *parse_configs)
    # some helpful debug when building
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
    # read module's scons script file
    module_dir = Dir(modname)
    built[modname] = SConscript(module_dir.File("scons.py"),
                                variant_dir = os.path.join(builder_helper.build_obj_path, modname),
                                duplicate = 0,
                                exports = ['env'])
    # copy module/etc content to build/etc
    copy_module_dir(modname, "etc")
    copy_module_dir(modname, "include")
    if verbose:
        print("")

###############################################################################
# Replace vars in files
###############################################################################

def sed_replace(file, replace_dic):
    for key, value in replace_dic.items():
        subprocess.call(['sed', '-i', 's/{}/{}/g'.format(key, value), file])

def fileinput_replace(file, replace_dic):
    for line in fileinput.input(file, inplace=True):
        for key, value in replace_dic.items():
            print(line.replace(key, value), end='')

def replace_res_in_build(to_replace, replace_dic):
    true_replace = []
    for pattern in to_replace:
        ret = glob.glob(os.path.join(str(build_dir), pattern), recursive = True)
        if ret:
            true_replace += ret
        else:
            true_replace.append(pattern)
    if verbose:
        builder_helper.debug("replacing values in build files - {} files to replace".format(len(true_replace)))
    for file in true_replace:
        if os.path.isfile(file) == False:
            builder_helper.warning("file to replace '{}' does not exists".format(file))
            continue
        if verbose:
            builder_helper.debug("replacing file: " + file)
        sed_replace(file, replace_dic)

# Replace every strings in specified files
if hasattr(app, "replace_files") and hasattr(app, "replace_vars"):
    replace_res_in_build(app.replace_files, app.replace_vars)

###############################################################################
# Scons progress build output
###############################################################################

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
    builder_helper.error("won't display progress - reason: " + str(e), file=sys.stderr)

###############################################################################
# Scons final build status
###############################################################################

import atexit

def build_failure_to_str(bf):
    """ Convert an element of GetBuildFailures() to a string in a useful way """
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
    """ Convert the build status to a 2-tuple, (status, msg) """
    from SCons.Script import GetBuildFailures
    build_failures = GetBuildFailures()
    if build_failures:
        # bf is normally a list of build failures; if an element is None,
        # it's because of a target that scons doesn't know anything about.
        success = False
        failures_message = "\n".join(["Failed building %s" % build_failure_to_str(x)
                                        for x in build_failures if x is not None])
    else:
        # if bf is None, the build completed successfully.
        success = True
        failures_message = ''
    return (success, failures_message)

def print_err(*args):
    print(*args, file=sys.stderr)

def display_build_status(success, failures_message):
    if not success:
        print_err("==============================================================")
        print_err("scons: BUILD FAILED (took {:.3f} sec)".format(time.time() - build_start_time))
        print_err(failures_message)
        print_err("==============================================================")
    else:
        builder_helper.info("build succeeded (took {:.3f} sec)".format(time.time() - build_start_time))

def after_build():
    success, failures_message = build_status()
    display_build_status(success, failures_message)
    if success and hasattr(app, "on_build_success"):
        app.on_build_success(build_modules.keys(), str(build_dir), str(lib_dir))
    elif hasattr(app, "on_build_fail"):
        app.on_build_fail(build_modules.keys())
    if builder_helper.build_for_windows:
        builder_helper.copy_dll_to_bin()
    if success and distribution:
        builder_helper.distribute_app(app)

atexit.register(after_build)