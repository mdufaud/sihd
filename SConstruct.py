if GetOption("help"):
    Return()

# General utilities
import glob
import sys
import os
import time
import fileinput
import subprocess
import shutil
# Pretty utility for verbosis
from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

###############################################################################
# App settings
###############################################################################

# Loading app configuration no bytecode for this
sys.dont_write_bytecode = True
import app
sys.dont_write_bytecode = False

from site_scons.scripts import modules
from site_scons.scripts import builder

builder.info("building {}".format(app.name))
builder.info("building to: {}".format(builder.build_path))

if builder.verify_args(app) == False:
    Exit(1)

###############################################################################
# Modules setup
###############################################################################

modules_to_build = builder.get_modules()
modules_forced_to_build = builder.get_force_build_modules()

modules_lst = modules_to_build.split(',')
if modules_forced_to_build:
    modules_lst.extend(modules_forced_to_build.split(','))

compiler = builder.build_compiler
build_platform = builder.build_platform

build_mode = builder.get_compile_mode()
distribution = builder.do_distribution()
verbose = builder.has_verbose()

if verbose:
    builder.info("modules: {}".format(modules_lst or "all"))
    builder.info("platform: " + build_platform)
    builder.info("compiler: " + compiler)
    builder.info("arch: " + builder.build_architecture)
    builder.info("mode: " + build_mode)
    builder.info("tests: " + (builder.build_tests and "yes" or "no"))
    builder.info("libraries: " + (builder.build_static_libs and "static" or "shared"))
    builder.info("address sanatizer: " + (builder.build_asan and "yes" or "no"))

# Get modules configuration for this build
try:
    build_modules = modules.build_modules_conf(app, specific_modules=modules_lst)
except RuntimeError as e:
    builder.error(str(e))
    Exit(1)

# Checking module availability on platforms
deleted_modules = modules.check_platform(build_modules, build_platform)
for deleted_modules in deleted_modules:
    builder.warning("module '{}' cannot compile on platform: {}".format(deleted_modules, build_platform))

if not build_modules:
    Exit(0)

###############################################################################
# Package manager dependencies
###############################################################################

pkg_manager_name = builder.get_pkgdep()
if pkg_manager_name:
    try:
        modules_extlibs = modules.get_modules_extlibs(app, build_modules, build_platform)
        packages, missing_packages = modules.get_modules_packages(app, pkg_manager_name, modules_extlibs)
        for missing in missing_packages:
            builder.warning("external library '{}' not declared in packet manager '{}'".format(missing, pkg_manager_name))
    except RuntimeError as e:
        builder.error(str(e))
        Exit(1)
    deps = set(packages.keys())
    if builder.build_tests:
        test_extlibs = modules.get_extlibs_versions(app, getattr(app, "test_extlibs", []))
        test_packages, missing_test_packages = modules.get_modules_packages(app, pkg_manager_name, test_extlibs)
        for missing in missing_test_packages:
            builder.warning("external library '{}' not declared in packet manager '{}'".format(missing, pkg_manager_name))
        deps.update(test_packages.keys())
    builder.info("dependencies:")
    print(" ".join(deps))
    Return()

###############################################################################
# Build environnement
###############################################################################

global_libs = getattr(app, "libs", [])
global_platform_libs = getattr(app, "{}_libs".format(build_platform), [])
if verbose:
    builder.debug("modules configuration:")
    pp.pprint(build_modules)
    if global_libs:
        builder.debug("libs:")
        pp.pprint(global_libs)
    if global_platform_libs:
        builder.debug("{} libs:".format(build_platform))
        pp.pprint(global_platform_libs)
    print()

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
    LINKFLAGS = builder.is_static_libs() and ["-static"] or [],
    # headers path
    CPPPATH = [builder.build_hdr_path, builder.build_extlib_hdr_path],
    # libraries path
    LIBPATH = [builder.build_lib_path, builder.build_extlib_lib_path],
    # libraries name
    LIBS = global_platform_libs + global_libs,
    # app access
    APP_CONFIG = app,
    # extra key for modules to build
    APP_MODULES_BUILD = build_modules.keys(),
)

if compiler != "mingw":
    base_env.Append(SHLIBVERSION = app.version)

base_env.Append(CPPDEFINES = builder.is_static_libs() and ["STATIC"] or [])

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

if not distribution and not compiler == "em" and not compiler == "mingw":
    base_env.Append(
        RPATH = [
            os.path.abspath(builder.build_lib_path),
            os.path.abspath(builder.build_extlib_lib_path)
        ],
    )

###############################################################################
# Compiler settings
###############################################################################

ccache = shutil.which("ccache") and "ccache " or ""
if verbose and ccache:
    builder.info("using ccache at: {}".format(shutil.which("ccache")))

if not ccache:
    # Cache compile build with md5
    scons_cache_path = os.path.join(builder.build_root_path, '.scons_cache')
    if verbose:
        builder.info("using scons cache at: {}".format(scons_cache_path))
    CacheDir(scons_cache_path)

compiler = builder.build_compiler
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
    if builder.build_asan:
        base_env.Append(
            CPPFLAGS = ["-fsanitize=address", "-fno-omit-frame-pointer"],
            LINKFLAGS = ["-fsanitize=address", "-fno-omit-frame-pointer"]
        )
    if builder.is_static_libs():
        base_env.ParseConfig("llvm-config --libs --system-libs --link-static")
    else:
        base_env.ParseConfig("llvm-config --libs --ldflags --system-libs")
# MINGW build
elif compiler == "mingw":
    base_env.Replace(
        CC = ccache + "x86_64-w64-mingw32-gcc",
        CXX = ccache + "x86_64-w64-mingw32-g++",
        AR = ccache + "x86_64-w64-mingw32-ar",
        RANLIB = ccache + "x86_64-w64-mingw32-ranlib",
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
    if builder.build_asan:
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

# General windows build
if builder.build_for_windows:
    base_env.Replace(
        SHLIBSUFFIX = ".dll",
        LIBSUFFIX = ".lib",
        LIBPREFIX = "",
    )

# add platform_[flags/defines/link/libs]
add_env_app_conf(app, base_env, builder.build_platform)
if build_mode:
    # add mode_[flags/defines/link/libs]
    add_env_app_conf(app, base_env, build_mode)

# add compiler_[flags/defines/link/libs]
add_env_app_conf(app, base_env, compiler)

###############################################################################
# Scons options
###############################################################################

num_jobs = GetOption("num_jobs")
if num_jobs <= 1:
    from multiprocessing import cpu_count
    num_jobs = cpu_count()
    if num_jobs > 3:
        num_jobs -= 2
    SetOption("num_jobs", num_jobs)

try:
    base_env.Tool('compilation_db')
    base_env["COMPILATIONDB_PATH_FILTER"] = os.path.join(builder.build_path, "*")
    base_env.CompilationDatabase()
except Exception as e:
    pass

# Decides when to recompile - removing slow md5 in favor of timestamps
Decider('timestamp-newer')

###############################################################################
# Build setup
###############################################################################

modules_generated_libs = {}
modules_generated_bins = {}
modules_generated_tests = {}
modules_generated_demos = {}
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

def _env_file_basename(self, path):
    return os.path.basename(os.path.splitext(str(path))[0])

def _env_build_demo(self, src, name = None, add_libs = [], **kwargs):
    """ Environment method to build unit test binary for a module """
    if builder.build_demo == False:
        return None
    # if modules are specified, demo is built only if specified by user and not automatically by dependencies
    if modules_to_build and self['APP_MODULE_NAME'] not in modules_to_build:
        return None
    global modules_generated_demos
    add_targets(src)
    if name is None:
        name = self["APP_MODULE_FORMAT_NAME"]
    demo_env = self.Clone()
    demo_env.Prepend(LIBS = add_libs)
    # add demo_[flags/defines/link/libs]
    add_env_app_conf(app, demo_env, "demo")
    # add compiler_demo_[flags/defines/link/libs]
    add_env_app_conf(app, demo_env, "{}_demo".format(compiler))
    # add platform_demo_[flags/defines/link/libs]
    add_env_app_conf(app, demo_env, "{}_demo".format(build_platform))
    demo_path = os.path.join(builder.build_demo_path, name)
    demo = demo_env.Program(demo_path, src, **kwargs)
    module_name = self['APP_MODULE_NAME']
    modules_generated_demos.setdefault(module_name, []).append(demo_path)
    return demo

def _env_build_demos(self, srcs, **kwargs):
    for src in srcs:
        name = self.file_basename(src)
        self.build_demo(src, name = name, **kwargs)

def _env_build_test(self, src, name = None, add_libs = [], **kwargs):
    """ Environment method to build unit test binary for a module """
    if builder.build_tests == False:
        return None
    # if modules are specified, test is built only if specified by user and not automatically by dependencies
    if modules_to_build and self['APP_MODULE_NAME'] not in modules_to_build:
        return None
    global modules_generated_tests
    add_targets(src)
    if name is None:
        name = self["APP_MODULE_FORMAT_NAME"]
    test_env = self.Clone()
    test_env.Prepend(LIBS = add_libs)
    # add test_[flags/defines/link/libs]
    add_env_app_conf(app, test_env, "test")
    # add compiler_test_[flags/defines/link/libs]
    add_env_app_conf(app, test_env, "{}_test".format(compiler))
    # add platform_test_[flags/defines/link/libs]
    add_env_app_conf(app, test_env, "{}_test".format(build_platform))
    test_path = os.path.join(builder.build_test_path, "bin", name)
    test = test_env.Program(test_path, src, **kwargs)
    module_name = self['APP_MODULE_NAME']
    modules_generated_tests.setdefault(module_name, []).append(test_path)
    return test

def _env_build_lib(self, src, name = None, static = None, **kwargs):
    """ Environment method to build a shared library for a module """
    global modules_generated_libs
    add_targets(src)
    if name is None:
        name = self["APP_MODULE_FORMAT_NAME"]
    lib_path = os.path.join(builder.build_lib_path, name)
    # no cache for symlinks renewal
    if (static is not None and static) or builder.build_static_libs:
        lib = NoCache(self.StaticLibrary(lib_path, src, **kwargs))
    else:
        lib = NoCache(self.SharedLibrary(lib_path, src, **kwargs))
    module_name = self['APP_MODULE_NAME']
    modules_generated_libs.setdefault(module_name, []).append(name)
    return lib

def _env_build_bin(self, src, name = None, add_libs = [], **kwargs):
    """ Environment method to build a binary for a module """
    global modules_generated_bins
    add_targets(src)
    if name is None:
        name = self['APP_MODULE_FORMAT_NAME']
    if build_platform == "windows":
        name += ".exe"
    bin_env = self.Clone()
    bin_env.Prepend(LIBS = add_libs)
    bin_path = os.path.join(builder.build_bin_path, name)
    bin = bin_env.Program(bin_path, src, **kwargs)
    module_name = self['APP_MODULE_NAME']
    modules_generated_bins.setdefault(module_name, []).append(bin_path)
    return bin

def _env_git_clone(self, url, branch, dest, recursive = False):
    global modules_cloned_git_repositories
    modname = self['APP_MODULE_NAME']
    if not os.path.isabs(dest):
        dest = os.path.join(builder.build_root_path, modname, dest)
    ret = True
    if os.path.isdir(dest):
        builder.info("repository already cloned: {}".format(dest))
    else:
        builder.info("cloning repository: {} -> {}".format(url, dest))
        args = ['git', 'clone']
        if recursive:
            args.append("--recursive")
        args.extend(['--branch', branch, '--depth', '1', url, dest])
        if verbose:
            builder.info("git clone cmd: '{}'".format(" ".join(args)))
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
        path = os.path.join(builder.build_root_path, module_name, path)
    builder.debug("replacing file: " + path)
    if not is_dry_run:
        _fileinput_replace(path, dic)

def _env_replace_in_build(self, to_replace, replace_dic):
    module_name = self['APP_MODULE_NAME']
    files_to_replace = []
    for pattern in to_replace:
        ret = glob.glob(os.path.join(builder.build_path, module_name, pattern), recursive = True)
        if ret:
            files_to_replace += ret
        else:
            files_to_replace.append(pattern)
    if verbose:
        builder.debug("replacing values in build files - {} files to replace".format(len(files_to_replace)))
    for path in files_to_replace:
        builder.debug("replacing file: " + path)
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
        src = os.path.join(builder.build_root_path, module_name, src)
    with open(src, 'r') as fd:
        for line in fd:
            if str in line:
                return True
    return False

# methods to build either test, lib or executable
base_env.AddMethod(_env_build_lib, "build_lib")
base_env.AddMethod(_env_build_bin, "build_bin")
base_env.AddMethod(_env_build_test, "build_test")
base_env.AddMethod(_env_build_demo, "build_demo")
base_env.AddMethod(_env_build_demos, "build_demos")
base_env.AddMethod(_env_pkg_config, "pkg_config")
base_env.AddMethod(_env_parse_config, "parse_config")
base_env.AddMethod(_env_replace_in_build, "build_replace")
base_env.AddMethod(_env_git_clone, "git_clone")
base_env.AddMethod(_env_get_module_env, "get_depends_env")
base_env.AddMethod(_env_copy_into_build, "copy_into_build")
base_env.AddMethod(_env_replace_in_file, "file_replace")
base_env.AddMethod(_env_find_in_file, "find_in_file")
base_env.AddMethod(_env_file_basename, "file_basename")
base_env.AddMethod(lambda self: self['APP_MODULE_NAME'], "module_name")
base_env.AddMethod(lambda self: self['APP_MODULE_FORMAT_NAME'], "module_format_name")
base_env.AddMethod(lambda self: self['APP_MODULE_CONF'], "module_conf")
base_env.AddMethod(lambda self: self['APP_MODULE_DIR'], "module_dir")
base_env.AddMethod(lambda self: self['APP_MODULES_BUILD'], "modules_to_build")
base_env.AddMethod(lambda self: builder, "builder")
base_env.AddMethod(lambda self: is_dry_run, "is_dry_run")

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
                + modules.get_module_libs(build_modules, modname)
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
        APP_MODULE_DIR = os.path.join(builder.build_root_path, modname)
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
    return env

###############################################################################
# Build
###############################################################################

# sorting build order by module dependencies size
modules_build_order = list(build_modules.values())
modules_build_order.sort(key = lambda obj: len(obj["depends"]))
# Configure env and call scons.py from every configured modules
built = {}
for conf in modules_build_order:
    modname = conf["modname"]
    builder.info("building module: {}".format(modname))
    env = get_module_env(conf,
        depends = conf.get("original-depends", []),
        append_depends_libs = conf.get("add-depends-libs", True),
        append_depends_defines = conf.get("add-depends-defines", True))
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
    copy_module_res_into_build(modname, "etc", "etc", must_exist = False)
    copy_module_res_into_build(modname, "include", "include", must_exist = False)
    copy_module_res_into_build(modname, "share", "share", must_exist = False)
    if builder.build_demo:
        copy_module_res_into_build(modname, "demo/etc", "etc", must_exist = False)
        copy_module_res_into_build(modname, "demo/include", "include", must_exist = False)
        copy_module_res_into_build(modname, "demo/share", "share", must_exist = False)
    # read module's scons script file
    module_format_name = env['APP_MODULE_FORMAT_NAME']
    built[modname] = SConscript(Dir(modname).File("scons.py"),
                                variant_dir = os.path.join(builder.build_obj_path, modname),
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

    def progress_function(node):
        if node not in targets:
            return
        global node_count
        node_count += 1
        if node_count_max > 0 :
            screen.write('\r[%3d%%] ' % (node_count * 100 / node_count_max))

    Progress(progress_function, interval = 1)
except (OSError, IOError) as e:
    builder.error("won't display progress - reason: " + str(e))

###############################################################################
# Scons after build
###############################################################################

import atexit

def display_built():
    for modname, binaries in modules_generated_bins.items():
        builder.info("module {} binaries:".format(modname))
        for binpath in binaries:
            builder.info("\t{}".format(binpath))

    for modname, demos in modules_generated_demos.items():
        builder.info("module {} demos:".format(modname))
        for demopath in demos:
            builder.info("\t{}".format(demopath))

    for modname, tests in modules_generated_tests.items():
        builder.info("module {} tests:".format(modname))
        for testpath in tests:
            builder.info("\t{}".format(testpath))

def display_build_status(success, failures_message):
    if not success:
        print_err("==============================================================")
        builder.error("BUILD FAILED (took {:.3f} sec)".format(time.time() - build_start_time))
        builder.error(failures_message)
        print_err("==============================================================")
    else:
        builder.info("build succeeded (took {:.3f} sec)".format(time.time() - build_start_time))

def after_build():
    success, failures_message = build_status()
    display_build_status(success, failures_message)
    if is_dry_run:
        return
    if success and hasattr(app, "on_build_success"):
        display_built()
        app.on_build_success(build_modules, builder)
    elif hasattr(app, "on_build_fail"):
        app.on_build_fail(build_modules, builder)
    builder.symlink_build()
    builder.copy_dll_to_build(modules_build_order)
    if success and distribution:
        builder.distribute_app(app, build_modules)

atexit.register(after_build)

builder.info("starting scons build (took {:.3f} sec)".format(time.time() - build_start_time))