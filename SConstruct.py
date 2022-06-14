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

if builder_helper.verify_args() == False:
    Exit(1)

builder_helper.sanitize_app(app)

###############################################################################
# Build settings
###############################################################################

modules_to_build = builder_helper.get_modules()
modules_forced_to_build = builder_helper.get_force_build_modules()

modules_lst = modules_to_build.split(',')
if modules_forced_to_build:
    modules_lst.extend(modules_forced_to_build.split(','))

compiler = builder_helper.build_compiler
build_platform = builder_helper.build_platform

distribution = builder_helper.do_distribution()
verbose = builder_helper.has_verbose()

if verbose:
    builder_helper.info("modules: {}".format(modules_lst or "all"))
    builder_helper.info("platform: " + build_platform)
    builder_helper.info("compiler: " + compiler)
    builder_helper.info("arch: " + builder_helper.build_architecture)
    builder_helper.info("mode: " + builder_helper.build_mode)
    builder_helper.info("tests: " + (builder_helper.build_tests and "yes" or "no"))
    builder_helper.info("libraries: " + (builder_helper.build_static_libs and "static" or "shared"))
    builder_helper.info("address sanatizer: " + (builder_helper.build_asan and "yes" or "no"))

# Get modules configuration for this build
try:
    build_modules = modules_helper.build_modules_conf(app, specific_modules=modules_lst)
except RuntimeError as e:
    builder_helper.error(str(e))
    Exit(1)
global_libs = getattr(app, "libs", [])
global_platform_libs = getattr(app, "{}_libs".format(build_platform), [])
if verbose:
    builder_helper.debug("modules configuration:")
    pp.pprint(build_modules)
    if global_libs:
        builder_helper.debug("libs:")
        pp.pprint(global_libs)
    if global_platform_libs:
        builder_helper.debug("{} libs:".format(build_platform))
        pp.pprint(global_platform_libs)
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
    LIBS = global_platform_libs + global_libs,
    # app access
    APP_CONFIG = app,
    # extra key for modules to build
    APP_MODULES_BUILD = build_modules.keys(),
    # builder helper for sconscript
    BUILDER_HELPER = builder_helper,
    # lib version + automatic symbolic links
    SHLIBVERSION = app.version
)
if build_platform == "windows":
    base_env.Append(LIBPATH = builder_helper.build_extlib_bin_path)

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

if not distribution and \
        not compiler == "em":
    base_env.Append(
        RPATH = [
            os.path.abspath(builder_helper.build_lib_path),
            os.path.abspath(builder_helper.build_extlib_lib_path)
        ],
    )

if builder_helper.build_mode == "debug":
    add_env_app_conf(base_env, "debug")
elif builder_helper.build_mode == "release":
    add_env_app_conf(base_env, "release")

# CLANG build
if compiler == "clang":
    base_env.Replace(
        # compiler for c
        CC = "clang",
        # compiler for c++
        CXX = "clang++",
        # static library archiver
        AR = "ar",
        # static library indexer
        RANLIB = "ranlib",
    )
    if builder_helper.build_asan:
        base_env.Append(
            CPPFLAGS = ["-fsanitize=address", "-fno-omit-frame-pointer"],
            LINKFLAGS = ["-fsanitize=address", "-fno-omit-frame-pointer"]
        )
    base_env.ParseConfig("llvm-config --libs --ldflags --system-libs")
    add_env_app_conf(base_env, "clang")

# MINGW build
elif compiler == "mingw":
    base_env.Replace(
        CC = "x86_64-w64-mingw32-gcc",
        CXX = "x86_64-w64-mingw32-g++",
        AR = "x86_64-w64-mingw32-ar",
        RANLIB = "x86_64-w64-mingw32-ranlib",
    )
    if not builder_helper.build_static_libs:
        base_env.Replace(
            SHLIBSUFFIX = ".dll",
            LIBPREFIX = "",
        )
    add_env_app_conf(base_env, "mingw")
# GCC build
elif compiler == "gcc":
    base_env.Replace(
        # compiler for c
        CC = "gcc",
        # compiler for c++
        CXX = "c++",
        # static library archiver
        AR = "ar",
        # static library indexer
        RANLIB = "ranlib",
    )
    if builder_helper.build_asan:
        base_env.Append(
            CPPFLAGS = ["-fsanitize=address", "-fno-omit-frame-pointer"],
            LINKFLAGS = ["-fsanitize=address", "-fno-omit-frame-pointer"]
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

# Cache compile build with md5
CacheDir('{}/.build_cache'.format(builder_helper.build_root_path))

###############################################################################
# Build
###############################################################################

modules_generated_libs = {}
modules_generated_bins = {}
modules_scons_libs = {}
modules_scons_bins = {}
modules_cloned_git_repositories = {}
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

## modules environnement methods

def _env_parse_config(self, config):
    """ Environment method to add pkg-config dynamically for a module """
    return load_env_packages_config(self, [config])

def _env_build_test(self, src, test_name=None, add_libs=[], **kwargs):
    """ Environment method to build unit test binary for a module """
    if builder_helper.build_tests == False:
        return None
    if modules_to_build and self['APP_MODULE_NAME'] not in modules_to_build:
        return None
    add_targets(src)
    if test_name is None:
        test_name = self["APP_MODULE_FORMAT_NAME"]
    test_env = self.Clone()
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

def _env_build_lib(self, src, lib_name=None, static=None, **kwargs):
    """ Environment method to build a shared library for a module """
    global modules_generated_libs
    global modules_scons_libs
    add_targets(src)
    module_name = self["APP_MODULE_FORMAT_NAME"]
    if lib_name is None:
        lib_name = module_name
    lib_path = os.path.join(builder_helper.build_lib_path, lib_name)
    # no cache for symlinks renewal
    if (static is not None and static) or builder_helper.build_static_libs:
        lib = NoCache(self.StaticLibrary(lib_path, src, **kwargs))
    else:
        lib = NoCache(self.SharedLibrary(lib_path, src, **kwargs))
    module_name = self['APP_MODULE_NAME']
    modules_generated_libs.setdefault(module_name, []).append(lib_name)
    modules_scons_libs[module_name] = lib
    return lib

def _env_build_bin(self, src, bin_name=None, add_libs=[], **kwargs):
    """ Environment method to build a binary for a module """
    global modules_generated_bins
    global modules_scons_bins
    add_targets(src)
    if bin_name is None:
        bin_name = self['APP_MODULE_FORMAT_NAME']
    if compiler == "mingw":
        bin_name += ".exe"
    bin_env = self.Clone()
    bin_env.Prepend(LIBS = add_libs)
    bin_path = os.path.join(builder_helper.build_bin_path, bin_name)
    bin = bin_env.Program(bin_path, src, **kwargs)
    module_name = self['APP_MODULE_NAME']
    modules_generated_bins.setdefault(module_name, []).append(bin_name)
    modules_scons_libs[module_name] = bin
    return bin

def _env_git_clone(self, url, branch, dest, recursive = False):
    global modules_cloned_git_repositories
    modname = self['APP_MODULE_NAME']
    dest = os.path.join(builder_helper.build_root_path, modname, dest)
    ret = True
    if os.path.isdir(dest):
        builder_helper.info("repository already cloned: {}".format(dest))
    else:
        builder_helper.info("cloning repository: {} -> {}".format(url, dest))
        args = ['git', 'clone']
        if recursive:
            args.append("--recursive")
        args.extend(['--branch', branch, '--depth', '1', url, dest])
        if verbose:
            builder_helper.info("git clone cmd: '{}'".format(" ".join(args)))
        ret = subprocess.call(args) == 0
    if ret:
        modules_cloned_git_repositories.setdefault(modname, []).append(dest)
    return ret

def _sed_replace(file, replace_dic):
    for key, value in replace_dic.items():
        subprocess.call(['sed', '-i', 's/{}/{}/g'.format(key, value), file])

def _fileinput_replace(file, replace_dic):
    for line in fileinput.input(file, inplace=True):
        for key, value in replace_dic.items():
            print(line.replace(key, value), end='')

def _env_replace_in_build(self, to_replace, replace_dic):
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
        builder_helper.debug("replacing file: " + file)
        _sed_replace(file, replace_dic)

def _env_get_module_env(self, depends = []):
    return get_module_env(self["APP_MODULE_CONF"], depends)

# methods to build either test, lib or executable
base_env.AddMethod(_env_build_lib, "build_lib")
base_env.AddMethod(_env_build_bin, "build_bin")
base_env.AddMethod(_env_build_test, "build_test")
base_env.AddMethod(_env_parse_config, "parse_config")
base_env.AddMethod(_env_replace_in_build, "build_replace")
base_env.AddMethod(_env_git_clone, "git_clone")
base_env.AddMethod(_env_get_module_env, "get_depends_env")

## build utilities

def load_env_packages_config(env, *configs):
    """ Parse multiple pkg-configs: libraries/includes utilities """
    if build_platform == "windows" or compiler == "em":
        return False
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

def copy_module_dir_into_build(module_name, dirname_to_copy):
    """ recursive copy of MODNAME/DIRNAME to build/DIRNAME """
    module_dir_input = Dir(module_name).Dir(dirname_to_copy)
    build_dir_output = build_dir.Dir(dirname_to_copy)
    if os.path.isdir(str(module_dir_input)):
        if verbose:
            builder_helper.info("copying resources '{}' of module: {}".format(dirname_to_copy, module_name))
        shutil.copytree(module_dir_input.get_abspath(), build_dir_output.get_abspath(), dirs_exist_ok = True)

def get_module_env(conf, depends = []):
    modname = conf["modname"]
    module_format = "{}_{}".format(app.name, modname)
    # get platform dependent libs
    platform_libs = conf.get("{}-libs".format(build_platform), [])
    compiler_libs = conf.get("{}-libs".format(compiler), [])
    # add flag
    flags = conf.get("flags", [])
    platform_flags = conf.get("{}-flags".format(build_platform), [])
    compiler_flags = conf.get("{}-flags".format(compiler), [])
    # add linkflags
    link = conf.get("link", [])
    platform_link = conf.get("{}-link".format(build_platform), [])
    compiler_link = conf.get("{}-link".format(compiler), [])
    # add libs generated by parent modules
    depends_generated_libs = []
    for dep_modname in depends:
        for generated_lib_name in modules_generated_libs.get(dep_modname, []):
            depends_generated_libs.insert(0, generated_lib_name)
    # Create a specific environment for the module
    env = base_env.Clone()
    # no need to add headers as they are copied to build
    env.Prepend(
        # adding libraries
        LIBS = depends_generated_libs
                + modules_helper.get_module_libs(build_modules, modname)
                + platform_libs
                + compiler_libs,
    )
    env.Append(
        # adding specified flags
        CPPFLAGS = flags + platform_flags + compiler_flags,
        # adding specified link flags
        LINKFLAGS = link + platform_link + compiler_link,
        # module name
        APP_MODULE_NAME = modname,
        # formatted module name PROJNAME_MODULENAME
        APP_MODULE_FORMAT_NAME = module_format,
        # configuration of module for scons.py
        APP_MODULE_CONF = conf,
        APP_MODULE_DIR = os.path.abspath(str(Dir(modname)))
    )
    # use multiple pkg-config output to add libraries/includes path
    package_configs = conf.get("pkg-configs", [])
    load_env_packages_config(env, *package_configs)
    # use multiple binaries output to add libraries/includes path
    parse_configs = conf.get("parse-configs", [])
    load_env_packages_specific_config(env, *parse_configs)
    # transform LIBS configuration with parse-configs
    # to unique elements and reverse order to have good linkage
    final_lib = env['LIBS']
    new_final_lib = []
    for el in final_lib[::-1]:
        if el not in new_final_lib:
            new_final_lib.append(el)
    new_final_lib.reverse()
    env['LIBS'] = new_final_lib
    # remove duplicates while preserving order
    env['CPPFLAGS'] = list(dict.fromkeys(env['CPPFLAGS']))
    env['LINKFLAGS'] = list(dict.fromkeys(env['LINKFLAGS']))
    return env

## building modules

# sorting build order by module dependencies size
build_order = list(build_modules.values())
build_order.sort(key = lambda obj: len(obj["depends"]))
# Configure env and call scons.py from every configured modules
built = {}
for conf in build_order:
    modname = conf["modname"]
    builder_helper.info("building module: {}".format(modname))
    if conf.get("add-depends-libs", True) is True:
        env = get_module_env(conf, depends = conf.get("original-depends"))
    else:
        env = get_module_env(conf, depends = [])
    # some helpful debug when building
    if verbose:
        print("- needed libraries")
        pp.pprint([ str(s) for s in env['LIBS'] ])
        print("- needed headers")
        pp.pprint([ str(s) for s in env['CPPPATH'] ])
        package_configs = conf.get("pkg-configs", [])
        if package_configs:
            print("- needed packages configs")
            pp.pprint(package_configs)
        parse_configs = conf.get("parse-configs", [])
        if parse_configs:
            print("- needed specific packages configs")
            pp.pprint(parse_configs)
    # copy module/[etc|include|share] to build/[etc|include|share]
    copy_module_dir_into_build(modname, "etc")
    copy_module_dir_into_build(modname, "include")
    copy_module_dir_into_build(modname, "share")
    # read module's scons script file
    module_dir = Dir(modname)
    built[modname] = SConscript(module_dir.File("scons.py"),
                                variant_dir = os.path.join(builder_helper.build_obj_path, modname),
                                duplicate = 0,
                                exports = ['env'])
    # fill module configurations with what was actually used
    conf["libs"] = env['LIBS']
    conf["flags"] = env['CPPFLAGS']
    conf["link"] = env['LINKFLAGS']
    # update added paths
    base_env["CPPPATH"] = list(set(env["CPPPATH"]))
    if verbose:
        print("")

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
    builder_helper.error("won't display progress - reason: " + str(e))

###############################################################################
# Scons after build
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
        app.on_build_success(build_modules, builder_helper)
    elif hasattr(app, "on_build_fail"):
        app.on_build_fail(build_modules, builder_helper)
    builder_helper.finalize()
    if success and distribution:
        builder_helper.distribute_app(app, build_modules)

atexit.register(after_build)

builder_helper.info("starting scons build")