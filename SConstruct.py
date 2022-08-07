# Time
from posixpath import basename
import time
build_start_time = time.time()
# General utilities
import sys
import glob
import os
import fileinput
import subprocess
import shutil
import filecmp
# Pretty utility for verbosis
from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

# Loading app configuration no bytecode for this
sys.dont_write_bytecode = True
import app
sys.dont_write_bytecode = False
from _build_tools import modules as modules_helper
from _build_tools import builder as builder_helper

builder_helper.info("building {}".format(app.name))
builder_helper.info("building to: {}".format(builder_helper.build_path))

if builder_helper.verify_args(app) == False:
    Exit(1)

###############################################################################
# Build settings
###############################################################################

# scons options
is_dry_run = bool(GetOption("no_exec"))

modules_to_build = builder_helper.get_modules()
modules_forced_to_build = builder_helper.get_force_build_modules()
build_mode = builder_helper.get_compile_mode()

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
    builder_helper.info("mode: " + build_mode)
    builder_helper.info("tests: " + (builder_helper.build_tests and "yes" or "no"))
    builder_helper.info("libraries: " + (builder_helper.build_static_libs and "static" or "shared"))
    builder_helper.info("address sanatizer: " + (builder_helper.build_asan and "yes" or "no"))

# Get modules configuration for this build
try:
    build_modules = modules_helper.build_modules_conf(app, specific_modules=modules_lst)
except RuntimeError as e:
    builder_helper.error(str(e))
    Exit(1)

# Checking module availability on platforms
deleted_modules = modules_helper.check_platform(build_modules, build_platform)
for deleted_modules in deleted_modules:
    builder_helper.warning("module '{}' cannot compile on platform: {}".format(deleted_modules, build_platform))

for deleted_module in deleted_modules:
    del build_modules[deleted_module]

if not build_modules:
    Exit(0)

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

pkg_manager_name = builder_helper.get_pkgdep()
if pkg_manager_name:
    try:
        modules_extlibs = modules_helper.get_modules_extlibs(app, build_modules, build_platform)
        packages, missing_packages = modules_helper.get_modules_packages(app, pkg_manager_name, modules_extlibs)
        for missing in missing_packages:
            builder_helper.warning("external library '{}' not declared in packet manager '{}'".format(missing, pkg_manager_name))
    except RuntimeError as e:
        builder_helper.error(str(e))
        Exit(1)
    deps = set(packages.keys())
    if builder_helper.build_tests:
        test_extlibs = modules_helper.get_extlibs_versions(app, getattr(app, "test_extlibs", []))
        test_packages, missing_test_packages = modules_helper.get_modules_packages(app, pkg_manager_name, test_extlibs)
        for missing in missing_test_packages:
            builder_helper.warning("external library '{}' not declared in packet manager '{}'".format(missing, pkg_manager_name))
        deps.update(test_packages.keys())
    builder_helper.info("dependencies: " + " ".join(deps))
    deps_str = " ".join(deps)
    builder_helper.info("install with:")
    if pkg_manager_name == "pacman":
        print("sudo pacman -Syu && sudo pacman -S {}".format(deps_str))
    else:
        print("sudo {} update && sudo {} install {}".format(pkg_manager_name, pkg_manager_name, deps_str))
    Exit(0)

###############################################################################
# Build compiling options
###############################################################################

__env_to_key = {
    "LIBS": "_libs",
    "CFLAGS": "_c_flags",
    "CXXFLAGS": "_cxx_flags",
    "CPPFLAGS": "_flags",
    "CPPDEFINES": "_defines",
    "LINKFLAGS": "_link",
}
def add_env_app_conf(env, key):
    for envkey, to_concat in __env_to_key.items():
        concat = key + to_concat
        attr = getattr(app, concat, None)
        if attr is not None:
            #prepends
            env[envkey][:0] = attr

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
    # c compile flags
    CFLAGS = getattr(app, 'c_flags', []),
    # c++ compile flags
    CXXFLAGS = getattr(app, 'cxx_flags', []),
    # c and c++ compile flags
    CPPFLAGS = getattr(app, 'flags', []),
    # extra #define for inside the code
    CPPDEFINES = getattr(app, 'defines', []),
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
    # lib version + automatic symbolic links
    SHLIBVERSION = app.version
)

# add platform_[flags/defines/link/libs]
add_env_app_conf(base_env, builder_helper.build_platform)
if build_mode:
    # add mode_[flags/defines/link/libs]
    add_env_app_conf(base_env, build_mode)

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

if not distribution and not compiler == "em":
    base_env.Append(
        RPATH = [
            os.path.abspath(builder_helper.build_lib_path),
            os.path.abspath(builder_helper.build_extlib_lib_path)
        ],
    )

ccache = shutil.which("ccache") and "ccache " or ""
if verbose and ccache:
    builder_helper.info("using ccache at: {}".format(shutil.which("ccache")))

if not ccache:
    # Cache compile build with md5
    scons_cache_path = '{}/.scons_cache'.format(builder_helper.build_root_path)
    if verbose:
        builder_helper.info("using scons cache at: {}".format(scons_cache_path))
    CacheDir(scons_cache_path)

num_jobs = GetOption("num_jobs")
if num_jobs <= 1:
    from multiprocessing import cpu_count
    num_jobs = cpu_count()
    if num_jobs > 3:
        num_jobs -= 2
    SetOption("num_jobs", num_jobs)

# CLANG build
if compiler == "clang":
    base_env.Replace(
        # compiler for c
        CC = ccache + "clang",
        # compiler for c++
        CXX = ccache + "clang++",
        # static library archiver
        AR = ccache + "ar",
        # static library indexer
        RANLIB = ccache + "ranlib",
    )
    if builder_helper.build_asan:
        base_env.Append(
            CPPFLAGS = ["-fsanitize=address", "-fno-omit-frame-pointer"],
            LINKFLAGS = ["-fsanitize=address", "-fno-omit-frame-pointer"]
        )
    base_env.ParseConfig("llvm-config --libs --ldflags --system-libs")

# MINGW build
elif compiler == "mingw":
    base_env.Replace(
        CC = ccache + "x86_64-w64-mingw32-gcc",
        CXX = ccache + "x86_64-w64-mingw32-g++",
        AR = ccache + "x86_64-w64-mingw32-ar",
        RANLIB = ccache + "x86_64-w64-mingw32-ranlib",
    )
    if not builder_helper.build_static_libs:
        base_env.Replace(
            SHLIBSUFFIX = ".dll",
            LIBPREFIX = "",
        )
# GCC build
elif compiler == "gcc":
    base_env.Replace(
        # compiler for c
        CC = ccache + "gcc",
        # compiler for c++
        CXX = ccache + "c++",
        # static library archiver
        AR = ccache + "ar",
        # static library indexer
        RANLIB = ccache + "ranlib",
    )
    if builder_helper.build_asan:
        base_env.Append(
            CPPFLAGS = ["-fsanitize=address", "-fno-omit-frame-pointer"],
            LINKFLAGS = ["-fsanitize=address", "-fno-omit-frame-pointer"]
        )
# EMSCRIPTEN build
elif compiler == "em":
    base_env.Replace(
        CC = ccache + "emcc",
        CXX = ccache + "em++",
        AR = ccache + "emar",
        RANLIB = ccache + "emranlib",
    )

# add compiler_[flags/defines/link/libs]
add_env_app_conf(base_env, compiler)

try:
    base_env.Tool('compilation_db')
    base_env["COMPILATIONDB_PATH_FILTER"] = "{}/*".format(builder_helper.build_path)
    base_env.CompilationDatabase()
except Exception as e:
    pass

# Decides when to recompile - removing slow md5 in favor of timestamps
Decider('timestamp-newer')


###############################################################################
# Build
###############################################################################

modules_generated_libs = {}
modules_generated_bins = {}
modules_generated_tests = {}
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

def _env_copy_into_build(self, src, dst):
    module_name = self['APP_MODULE_NAME']
    copy_module_res_into_build(module_name, src, dst)

def _env_build_test(self, src, name=None, add_libs=[], **kwargs):
    """ Environment method to build unit test binary for a module """
    if builder_helper.build_tests == False:
        return None
    if modules_to_build and self['APP_MODULE_NAME'] not in modules_to_build:
        return None
    add_targets(src)
    if name is None:
        name = self["APP_MODULE_FORMAT_NAME"]
    test_env = self.Clone()
    test_env.Prepend(LIBS = add_libs)
    # add test_[flags/defines/link/libs]
    add_env_app_conf(test_env, "test")
    # add compiler_test_[flags/defines/link/libs]
    add_env_app_conf(test_env, "{}_test".format(compiler))
    # add platform_test_[flags/defines/link/libs]
    add_env_app_conf(test_env, "{}_test".format(build_platform))
    test_path = os.path.join(builder_helper.build_test_path, "bin", name)
    test = test_env.Program(test_path, src, **kwargs)
    module_name = self['APP_MODULE_NAME']
    modules_generated_tests.setdefault(module_name, []).append(test_path)
    return test

def _env_build_lib(self, src, name=None, static=None, **kwargs):
    """ Environment method to build a shared library for a module """
    global modules_generated_libs
    add_targets(src)
    if name is None:
        name = self["APP_MODULE_FORMAT_NAME"]
    lib_path = os.path.join(builder_helper.build_lib_path, name)
    # no cache for symlinks renewal
    if (static is not None and static) or builder_helper.build_static_libs:
        lib = NoCache(self.StaticLibrary(lib_path, src, **kwargs))
    else:
        lib = NoCache(self.SharedLibrary(lib_path, src, **kwargs))
    module_name = self['APP_MODULE_NAME']
    modules_generated_libs.setdefault(module_name, []).append(name)
    return lib

def _env_build_bin(self, src, name=None, add_libs=[], **kwargs):
    """ Environment method to build a binary for a module """
    global modules_generated_bins
    add_targets(src)
    if name is None:
        name = self['APP_MODULE_FORMAT_NAME']
    if build_platform == "windows":
        name += ".exe"
    bin_env = self.Clone()
    bin_env.Prepend(LIBS = add_libs)
    bin_path = os.path.join(builder_helper.build_bin_path, name)
    bin = bin_env.Program(bin_path, src, **kwargs)
    module_name = self['APP_MODULE_NAME']
    modules_generated_bins.setdefault(module_name, []).append(bin_path)
    return bin

def _env_git_clone(self, url, branch, dest, recursive = False):
    global modules_cloned_git_repositories
    modname = self['APP_MODULE_NAME']
    if not os.path.isabs(dest):
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
        if not is_dry_run:
            ret = subprocess.call(args) == 0
    if ret:
        modules_cloned_git_repositories.setdefault(modname, []).append(dest)
    return ret

def _sed_replace(path, replace_dic):
    if not os.path.isfile(path):
        raise RuntimeError("File to replace {} does not exists".format(path))
    for key, value in replace_dic.items():
        subprocess.call(['sed', '-i', 's/{}/{}/g'.format(key, value), path])

def _fileinput_replace(path, replace_dic):
    if not os.path.isfile(path):
        raise RuntimeError("File to replace {} does not exists".format(path))
    for line in fileinput.input(path, inplace=True):
        for key, value in replace_dic.items():
            print(line.replace(key, value), end='')

def _env_replace_in_file(self, path, dic):
    module_name = self['APP_MODULE_NAME']
    if not os.path.isabs(path):
        path = os.path.join(builder_helper.build_root_path, module_name, path)
    builder_helper.debug("replacing file: " + path)
    if not is_dry_run:
        _fileinput_replace(path, dic)

def _env_replace_in_build(self, to_replace, replace_dic):
    module_name = self['APP_MODULE_NAME']
    files_to_replace = []
    for pattern in to_replace:
        ret = glob.glob(os.path.join(builder_helper.build_path, module_name, pattern), recursive = True)
        if ret:
            files_to_replace += ret
        else:
            files_to_replace.append(pattern)
    if verbose:
        builder_helper.debug("replacing values in build files - {} files to replace".format(len(files_to_replace)))
    for path in files_to_replace:
        builder_helper.debug("replacing file: " + path)
        if not is_dry_run:
            _sed_replace(path, replace_dic)

def _env_get_module_env(self, **kwargs):
    return get_module_env(self["APP_MODULE_CONF"], **kwargs)

def _env_pkg_config(self, config):
    """ Environment method to add pkg-config dynamically for a module """
    return load_env_packages_config(self, [config])

def _env_parse_config(self, config):
    """ Environment method to add lib's binary-config dynamically for a module """
    return parse_config_command(self, [config])

def _env_find_in_file(self, src, str):
    module_name = self['APP_MODULE_NAME']
    if not os.path.isabs(src):
        src = os.path.join(builder_helper.build_root_path, module_name, src)
    with open(src, 'r') as fd:
        for line in fd:
            if str in line:
                return True
    return False

# methods to build either test, lib or executable
base_env.AddMethod(_env_build_lib, "build_lib")
base_env.AddMethod(_env_build_bin, "build_bin")
base_env.AddMethod(_env_build_test, "build_test")
base_env.AddMethod(_env_pkg_config, "pkg_config")
base_env.AddMethod(_env_parse_config, "parse_config")
base_env.AddMethod(_env_replace_in_build, "build_replace")
base_env.AddMethod(_env_git_clone, "git_clone")
base_env.AddMethod(_env_get_module_env, "get_depends_env")
base_env.AddMethod(_env_copy_into_build, "copy_into_build")
base_env.AddMethod(_env_replace_in_file, "file_replace")
base_env.AddMethod(_env_find_in_file, "find_in_file")
base_env.AddMethod(lambda self: self['APP_MODULE_NAME'], "module_name")
base_env.AddMethod(lambda self: self['APP_MODULE_FORMAT_NAME'], "module_format_name")
base_env.AddMethod(lambda self: self['APP_MODULE_CONF'], "module_conf")
base_env.AddMethod(lambda self: self['APP_MODULE_DIR'], "module_dir")
base_env.AddMethod(lambda self: self['APP_MODULES_BUILD'], "modules_to_build")
base_env.AddMethod(lambda self: builder_helper, "builder_helper")
base_env.AddMethod(lambda self: is_dry_run, "is_dry_run")

## build utilities
def load_env_packages_config(env, *configs):
    """ Parse multiple pkg-configs: libraries/includes utilities """
    return parse_config_command(env, [
        "pkg-config {} --cflags --libs".format(config)
        for config in configs
    ])

def parse_config_command(env, *configs):
    """ Parse configs from binaries outputs """
    if build_platform == "windows" or compiler == "em":
        return False
    for config in configs:
        try:
            env.ParseConfig(config)
        except OSError as e:
            builder_helper.warning("config '{}' not found".format(config))

def copy_module_res_into_build(module_name, src, dst, must_exist = True):
    """ recursive copy of MODNAME/DIRNAME to build/DIRNAME """
    src = str(src)
    dst = str(dst)
    module_res = os.path.join(builder_helper.build_root_path, module_name, src)
    build_output = os.path.join(builder_helper.build_path, dst)
    if verbose:
        builder_helper.info("copying resources of module {}/{} -> build/{}".format(module_name, src, dst))
    if os.path.isfile(module_res):
        compare_output_file = None
        if os.path.isfile(build_output):
            compare_output_file = build_output
        elif os.path.isdir(build_output):
            compare_output_file = os.path.join(build_output, os.path.basename(module_res))
        if compare_output_file and os.path.isfile(compare_output_file) and filecmp.cmp(module_res, compare_output_file):
            # file is same
            return
        if not is_dry_run:
            shutil.copy(module_res, build_output)
    elif os.path.isdir(module_res):
        if not is_dry_run:
            shutil.copytree(module_res, build_output, dirs_exist_ok = True)
    elif must_exist:
        raise RuntimeError("for module {} resource {} not found".format(module_name, module_res))

def get_module_env(conf, depends = [], append_depends_libs = True, append_depends_defines = True):
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
    # add defines
    defines = conf.get("defines", [])
    platform_defines = conf.get("{}-defines".format(build_platform), [])
    compiler_defines = conf.get("{}-defines".format(compiler), [])
    # add libs generated by parent modules
    depends_generated_libs = []
    depends_defines = []
    for dep_modname in depends:
        if append_depends_libs:
            for generated_lib_name in modules_generated_libs.get(dep_modname, []):
                depends_generated_libs.insert(0, generated_lib_name)
        if append_depends_defines:
            depend_conf = build_modules.get(dep_modname, {})
            if depend_conf:
                depends_defines.extend(depend_conf.get("defines", []))
    # Create a specific environment for the module
    env = base_env.Clone()
    # no need to add headers as they are copied to build
    env.PrependUnique(
        # adding libraries
        LIBS = depends_generated_libs
                + modules_helper.get_module_libs(build_modules, modname)
                + platform_libs
                + compiler_libs,
    )
    env.AppendUnique(
        # adding specified flags
        CPPFLAGS = flags + platform_flags + compiler_flags,
        # adding specified link flags
        LINKFLAGS = link + platform_link + compiler_link,
        # adding defines
        CPPDEFINES = defines + platform_defines + compiler_defines + depends_defines,
        # module name
        APP_MODULE_NAME = modname,
        # formatted module name PROJNAME_MODULENAME
        APP_MODULE_FORMAT_NAME = module_format,
        # configuration of module for scons.py
        APP_MODULE_CONF = conf,
        APP_MODULE_DIR = os.path.join(builder_helper.build_root_path, modname)
    )
    # use multiple pkg-config output to add libraries/includes path
    package_configs = conf.get("pkg-configs", [])
    load_env_packages_config(env, *package_configs)
    # use multiple binaries output to add libraries/includes path
    parse_configs = conf.get("parse-configs", [])
    parse_config_command(env, *parse_configs)
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
    # env['CPPDEFINES'] = list(set(env['CPPDEFINES']))
    # env['CPPFLAGS'] = list(dict.fromkeys(env['CPPFLAGS']))
    # env['LINKFLAGS'] = list(dict.fromkeys(env['LINKFLAGS']))
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
    env = get_module_env(conf,
        depends = conf.get("original-depends", []),
        append_depends_libs=conf.get("add-depends-libs", True),
        append_depends_defines=conf.get("add-depends-defines", True))
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
    copy_module_res_into_build(modname, "etc", "etc", must_exist=False)
    copy_module_res_into_build(modname, "include", "include", must_exist=False)
    copy_module_res_into_build(modname, "share", "share", must_exist=False)
    # read module's scons script file
    module_format_name = env['APP_MODULE_FORMAT_NAME']
    built[modname] = SConscript(Dir(modname).File("scons.py"),
                                variant_dir = os.path.join(builder_helper.build_obj_path, modname),
                                duplicate = 0,
                                exports = ['env'])
    # fill module configurations with what was actually used
    conf["libs"] = env['LIBS']
    conf["flags"] = env['CPPFLAGS']
    conf["link"] = env['LINKFLAGS']
    conf["defines"] = env["CPPDEFINES"]
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
        builder_helper.error("BUILD FAILED (took {:.3f} sec)".format(time.time() - build_start_time))
        builder_helper.error(failures_message)
        print_err("==============================================================")
    else:
        builder_helper.info("build succeeded (took {:.3f} sec)".format(time.time() - build_start_time))

def display_built():
    for modname, binaries in modules_generated_bins.items():
        builder_helper.info("module {} binaries:".format(modname))
        for binpath in binaries:
            builder_helper.info("\t{}".format(binpath))
    for modname, tests in modules_generated_tests.items():
        builder_helper.info("module {} tests:".format(modname))
        for testpath in tests:
            builder_helper.info("\t{}".format(testpath))

def after_build():
    success, failures_message = build_status()
    display_build_status(success, failures_message)
    if is_dry_run:
        return
    if success and hasattr(app, "on_build_success"):
        display_built()
        app.on_build_success(build_modules, builder_helper)
    elif hasattr(app, "on_build_fail"):
        app.on_build_fail(build_modules, builder_helper)
    builder_helper.finalize()
    if success and distribution:
        builder_helper.distribute_app(app, build_modules)

atexit.register(after_build)

builder_helper.info("starting scons build (took {:.3f} sec)".format(time.time() - build_start_time))