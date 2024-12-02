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
from site_scons.scripts import scons_utils

builder.info("building {}".format(app.name))
builder.info("building to: {}".format(builder.build_path))

if builder.verify_args(app) == False:
    Exit(1)

###############################################################################
# Modules setup
###############################################################################

modules_to_build = builder.get_modules()
modules_forced_to_build = builder.get_force_build_modules()

modules_lst = builder.get_modules_lst()
if modules_forced_to_build:
    modules_lst.extend(builder.get_force_build_modules_lst())

compiler = builder.build_compiler
build_platform = builder.build_platform

build_mode = builder.get_compile_mode()
distribution = builder.do_distribution()
verbose = builder.has_verbose()

libtype = builder.build_static_libs and "static" or "dyn"
bin_ext = builder.build_compiler == "em" and ".html" or (build_platform == "windows" and ".exe" or "")

compile_commands = builder.is_opt("compile_commands", "1")

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
    ENV = os.environ,
    # c compile flags
    CFLAGS = getattr(app, 'c_flags', []),
    # c++ compile flags
    CXXFLAGS = getattr(app, 'cxx_flags', []),
    # c and c++ compile flags
    CPPFLAGS = getattr(app, 'flags', []),
    # extra #define for inside the code
    CPPDEFINES = getattr(app, 'defines', []),
    # link flags
    LINKFLAGS = getattr(app, 'link', []),
    # headers path
    CPPPATH = [builder.build_hdr_path, builder.build_extlib_hdr_path],
    # libraries path
    LIBPATH = [builder.build_lib_path, builder.build_extlib_lib_path],
    # libraries name
    LIBS = global_platform_libs + global_libs,
    # extra key for modules to build
    APP_MODULES_BUILD = build_modules.keys(),
    COMPILATIONDB_USE_ABSPATH = True,
    # scons tools
    tools = [
        # general utils
        'tar',
        'zip',
        'textfile',
        # linux c compiler
        'gcc',
        'cc',
        # linux c++ compiler
        'g++',
        'cxx',
        # linux assembly compiler
        'nasm',
        # linux linker
        'gnulink',
        # linux static archiver
        'ar'
    ]
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

if not distribution \
    and not compiler in ("mingw", "em"):
    base_env.Append(
        RPATH = [
            os.path.abspath(builder.build_lib_path),
            os.path.abspath(builder.build_extlib_lib_path)
        ],
    )

if build_platform == "windows":
    base_env.Append(
        LIBPATH = [builder.build_extlib_bin_path]
    )

###############################################################################
# Compiler settings
###############################################################################

ccache = ""
if not builder.is_opt("nocache"):
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
        CC = "clang",
        # compiler for c++
        CXX = "clang++",
        # static library archiver
        AR = "ar",
        # static library indexer
        RANLIB = "ranlib",
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
        CC = "x86_64-w64-mingw32-gcc",
        CXX = "x86_64-w64-mingw32-g++",
        AR = "x86_64-w64-mingw32-ar",
        RANLIB = "x86_64-w64-mingw32-ranlib",
    )
    base_env.Replace(
        SHLIBSUFFIX = ".dll",
        LIBSUFFIX = ".lib",
        LIBPREFIX = "",
    )
    if builder.is_static_libs():
        base_env.Append(
            LINKFLAGS = ["-static", "-static-libgcc", "-static-libstdc++"]
        )
# GCC build
elif compiler == "gcc":
    base_env.Replace(
        CC = "gcc",
        CXX = "c++",
        AR = "ar",
        RANLIB = "ranlib",
    )
    if builder.build_asan:
        base_env.Append(
            CPPFLAGS = ["-fsanitize=address", "-fno-omit-frame-pointer"],
            LINKFLAGS = ["-fsanitize=address", "-fno-omit-frame-pointer"]
        )
    if builder.is_static_libs():
        base_env.Append(
            LINKFLAGS = ["-static", "-static-libgcc", "-static-libstdc++"]
        )
# EMSCRIPTEN build
elif compiler == "em":
    base_env.Replace(
        CC = "emcc",
        CXX = "em++",
        AR = "emar",
        RANLIB = "emranlib",
        LINK = "emcc",
    )
    emscripten_conf = os.path.join(os.getenv("HOME"), ".emscripten")
    if os.path.isfile(emscripten_conf):
        try:
            exec(open(emscripten_conf).read())
        except Exception as e:
            builder.warning("could not execute emscripten configuration: " + emscripten_conf)

base_env.Prepend(
    CC = ccache,
    CXX = ccache,
    AR = ccache,
    RANLIB = ccache,
)

if compiler != "mingw" and not builder.is_msys():
    base_env.Replace(SHLIBVERSION = app.version)

# add $platform-[flags/defines/link/libs]
# add $libtype-[flags/defines/link/libs]
# add $mode-[flags/defines/link/libs]
# add $compiler-[flags/defines/link/libs]
# add $platform--[flags/defines/link/libs]

if verbose:
    builder.debug(f"looking for app configurations:")

default_app_conf_to_get = (build_platform, libtype, build_mode, compiler)

def add_combinaison_app_conf_to_env(env):
    for idx, app_config in enumerate(default_app_conf_to_get, start=1):
        keys = [app_config]
        scons_utils.add_env_app_conf(app, env, *keys)
        for sub_app_config in default_app_conf_to_get[idx:]:
            keys.append(sub_app_config)
            scons_utils.add_env_app_conf(app, env, *keys)

add_combinaison_app_conf_to_env(base_env)

###############################################################################
# Scons options
###############################################################################

is_dry_run = bool(GetOption("no_exec"))

num_jobs = GetOption("num_jobs")
if num_jobs <= 1:
    from multiprocessing import cpu_count
    num_jobs = cpu_count()
    if num_jobs > 3:
        num_jobs -= 2
    SetOption("num_jobs", num_jobs)

if compile_commands:
    compilation_db_path = os.path.join(builder.build_path, "compile_commands.json")
    try:
        base_env.Tool('compilation_db')
        # base_env["COMPILATIONDB_PATH_FILTER"] = f"*[!{builder.build_path}].*"
        base_env.CompilationDatabase(compilation_db_path)
    except Exception as e:
        builder.warning(e)
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
    scons_utils.copy_module_res_into_build(module_name, src, dst, is_dry_run = is_dry_run)

def _env_file_basename(self, path):
    return os.path.basename(os.path.splitext(str(path))[0])

def _env_build_demo(self, src, name, add_libs = [], **kwargs):
    """ Environment method to build unit test binary for a module """
    if builder.build_demo == False:
        return None
    module_name = self['APP_MODULE_NAME']

    # if modules are specified, demo is built only if specified by user and not automatically by dependencies
    if modules_lst and module_name not in modules_lst:
        return None

    demo_env = self.Clone()
    demo_env.Prepend(LIBS = add_libs)
    scons_utils.add_env_app_conf(app, demo_env, "demo")
    for app_conf in default_app_conf_to_get:
        scons_utils.add_env_app_conf(app, demo_env, "demo", app_conf)

    name += bin_ext
    demo_path = os.path.join(builder.build_demo_path, name)
    demo = demo_env.Program(demo_path, src, **kwargs)

    add_targets(src)

    global modules_generated_demos
    modules_generated_demos.setdefault(module_name, []).append(demo_path)
    if verbose:
        builder.debug(f"module '{module_name}' registered demo: {demo_path}")
    return demo

def _env_build_demos(self, srcs, **kwargs):
    for src in srcs:
        name = self.file_basename(src)
        self.build_demo(src, name = name, **kwargs)

def _env_build_test(self, src, name = None, add_libs = [], **kwargs):
    """ Environment method to build unit test binary for a module """
    if builder.build_tests == False:
        return None
    module_name = self['APP_MODULE_NAME']

    # if modules are specified, test is built only if specified by user and not automatically by dependencies
    if modules_lst and module_name not in modules_lst:
        return None

    test_env = self.Clone()
    test_env.Prepend(LIBS = add_libs)
    scons_utils.add_env_app_conf(app, test_env, "test")
    for app_conf in default_app_conf_to_get:
        scons_utils.add_env_app_conf(app, test_env, "test", app_conf)

    if name is None:
        name = self["APP_MODULE_FORMAT_NAME"]
    name += bin_ext
    test_path = os.path.join(builder.build_test_path, "bin", name)
    test = test_env.Program(test_path, src, **kwargs)

    add_targets(src)

    global modules_generated_tests
    modules_generated_tests.setdefault(module_name, []).append(test_path)
    if verbose:
        builder.debug(f"module '{module_name}' registered test: {test_path}")
    return test

def _env_build_lib(self, src, name = None, static = None, **kwargs):
    """ Environment method to build a shared library for a module """
    module_name = self['APP_MODULE_NAME']

    if name is None:
        name = self["APP_MODULE_FORMAT_NAME"]
    lib_path = os.path.join(builder.build_lib_path, name)

    # no cache for symlinks renewal
    if (static is not None and static) or builder.build_static_libs:
        lib = NoCache(self.StaticLibrary(lib_path, src, **kwargs))
    else:
        lib = NoCache(self.SharedLibrary(lib_path, src, **kwargs))

    add_targets(src)

    global modules_generated_libs
    modules_generated_libs.setdefault(module_name, []).append(name)
    if verbose:
        builder.debug(f"module '{module_name}' registered lib: {name}")
    return lib

def _env_build_bin(self, src, name = None, add_libs = [], **kwargs):
    """ Environment method to build a binary for a module """
    module_name = self['APP_MODULE_NAME']

    bin_env = self.Clone()
    bin_env.Prepend(LIBS = add_libs)

    if name is None:
        name = self['APP_MODULE_FORMAT_NAME']

    name += bin_ext
    bin_path = os.path.join(builder.build_bin_path, name)
    bin = bin_env.Program(bin_path, src, **kwargs)

    add_targets(src)

    global modules_generated_bins
    modules_generated_bins.setdefault(module_name, []).append(bin_path)
    if verbose:
        builder.debug(f"module '{module_name}' registered bin: {bin_path}")
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
        if not is_dry_run or builder.force_git_clone():
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

def _env_create_module_env(self, **kwargs):
    return create_module_env(self["APP_MODULE_CONF"], **kwargs)

def _env_pkg_config(self, config):
    """ Environment method to add pkg-config dynamically for a module """
    return scons_utils.load_env_packages_config(self, [config])

def _env_parse_config(self, config):
    """ Environment method to add lib's binary-config dynamically for a module """
    return scons_utils.parse_config_command(self, [config])

def _env_find_in_file(self, src, str):
    module_name = self['APP_MODULE_NAME']
    if not os.path.isabs(src):
        src = os.path.join(builder.build_root_path, module_name, src)
    with open(src, 'r') as fd:
        for line in fd:
            if str in line:
                return True
    return False

def _env_filter_files(self, files_lst, filter_lst):
    def _filter_fun(file):
        return os.path.basename(str(file)) not in filter_lst
    return list(filter(_filter_fun, files_lst))

# methods to build either libs, tests, demos or executables
base_env.AddMethod(_env_build_lib, "build_lib")
base_env.AddMethod(_env_build_bin, "build_bin")
base_env.AddMethod(_env_build_test, "build_test")
base_env.AddMethod(_env_build_demo, "build_demo")
base_env.AddMethod(_env_build_demos, "build_demos")
base_env.AddMethod(_env_pkg_config, "pkg_config")
base_env.AddMethod(_env_parse_config, "parse_config")
base_env.AddMethod(_env_replace_in_build, "build_replace")
base_env.AddMethod(_env_git_clone, "git_clone")
base_env.AddMethod(_env_create_module_env, "create_module_env")
base_env.AddMethod(_env_copy_into_build, "copy_into_build")
base_env.AddMethod(_env_replace_in_file, "file_replace")
base_env.AddMethod(_env_find_in_file, "find_in_file")
base_env.AddMethod(_env_file_basename, "file_basename")
base_env.AddMethod(_env_filter_files, "filter_files")
base_env.AddMethod(lambda self: self['APP_MODULE_NAME'], "module_name")
base_env.AddMethod(lambda self: self['APP_MODULE_FORMAT_NAME'], "module_format_name")
base_env.AddMethod(lambda self: self['APP_MODULE_CONF'], "module_conf")
base_env.AddMethod(lambda self: self['APP_MODULE_DIR'], "module_dir")
base_env.AddMethod(lambda self: self['APP_MODULES_BUILD'], "modules_to_build")
base_env.AddMethod(lambda self: app, "app")
base_env.AddMethod(lambda self: builder, "builder")
base_env.AddMethod(lambda self: is_dry_run, "is_dry_run")

modules_options = [
    libtype,
    build_platform,
    f"mode-{build_mode}",
    compiler,
]

def _add_lib_type_to_modules_options():
    global modules_options
    to_append = []
    for option in modules_options:
        if option != libtype:
            to_append.append(f"{option}-{libtype}")
    modules_options += to_append

_add_lib_type_to_modules_options()

if verbose:
    builder.debug(f"will look for modules options:")
    pp.pprint([ [ f"{option}-{key}" for option in modules_options ] for key in ("libs", "flags", "link", "defines")])

def get_compilation_options(conf, key):
    ret = []
    for option in modules_options:
        val = conf.get(f"{option}-{key}", [])
        ret += val
    return ret

def create_module_env(conf, depends = [],
                        do_inherit_depends_libs = False,
                        do_inherit_depends_defines = False,
                        do_inherit_depends_links = False,
                        do_inherit_depends_flags = False,
                        do_inherit_depends_generated_libs = False,
                    ):
    modname = conf["modname"]
    module_format = "{}_{}".format(app.name, modname)
    # get platform dependent libs
    libs = modules.get_module_libs(build_modules, modname, add_depends_libs = do_inherit_depends_libs)
    libs += get_compilation_options(conf, "libs")
    # add flag
    flags = conf.get("flags", [])
    flags += get_compilation_options(conf, "flags")
    # add linkflags
    link = conf.get("link", [])
    link += get_compilation_options(conf, "link")
    # add defines
    defines = conf.get("defines", [])
    defines += get_compilation_options(conf, "defines")

    # add libs generated by parent modules
    depends_generated_libs = []
    depends_defines = []
    depends_links = []
    depends_flags = []
    for dep_modname in depends:
        if do_inherit_depends_generated_libs:
            for generated_lib_name in modules_generated_libs.get(dep_modname, []):
                depends_generated_libs.insert(0, generated_lib_name)
        depend_conf = build_modules.get(dep_modname, {})
        if depend_conf:
            if do_inherit_depends_defines:
                depends_defines.extend(depend_conf.get("defines", []))
            if do_inherit_depends_links:
                depends_links.extend(depend_conf.get("link", []))
            if do_inherit_depends_flags:
                depends_flags.extend(depend_conf.get("flags", []))

    # Create a specific environment for the module
    env = base_env.Clone()
    # no need to add headers as they are copied to build
    env.PrependUnique(
        # adding libraries
        LIBS = depends_generated_libs + libs
    )
    env.AppendUnique(
        # adding specified flags
        CPPFLAGS = flags + depends_flags,
        # adding specified link flags
        LINKFLAGS = link + depends_links,
        # adding defines
        CPPDEFINES = defines + depends_defines,
        # module name
        APP_MODULE_NAME = modname,
        # formatted module name PROJNAME_MODULENAME
        APP_MODULE_FORMAT_NAME = module_format,
        # configuration of module for scons.py
        APP_MODULE_CONF = conf,
        APP_MODULE_DIR = os.path.join(builder.build_root_path, modname),
    )
    # use multiple pkg-config output to add libraries/includes path
    package_configs = conf.get("pkg-configs", [])
    scons_utils.load_env_packages_config(env, *package_configs)
    # use multiple binaries output to add libraries/includes path
    parse_configs = conf.get("parse-configs", [])
    scons_utils.parse_config_command(env, *parse_configs)
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
    env = create_module_env(conf,
        depends = conf.get("original-depends", []),
        do_inherit_depends_libs = conf.get("inherit-depends-libs", False),
        do_inherit_depends_defines = conf.get("inherit-depends-defines", False),
        do_inherit_depends_links = conf.get("inherit-depends-links", False),
        do_inherit_depends_flags = conf.get("inherit-depends-flags", False),
        # it is quite obvious you might want the libs generated from your dependencies
        do_inherit_depends_generated_libs = conf.get("inherit-depends-generated-libs", True),
    )
    # some helpful debug when building
    if verbose:
        builder.debug("needed libraries:")
        pp.pprint([ str(s) for s in env['LIBS'] ])
        builder.debug("needed headers:")
        pp.pprint([ str(s) for s in env['CPPPATH'] ])
        package_configs = conf.get("pkg-configs", [])
        if package_configs:
            builder.debug("needed packages configs")
            pp.pprint(package_configs)
        parse_configs = conf.get("parse-configs", [])
        if parse_configs:
            builder.debug("needed specific packages configs")
            pp.pprint(parse_configs)
    # copy module/[etc|include|share] to build/[etc|include|share]
    scons_utils.copy_module_res_into_build(modname, "etc", "etc", must_exist=False, is_dry_run=is_dry_run)
    scons_utils.copy_module_res_into_build(modname, "include", "include", must_exist=False, is_dry_run=is_dry_run)
    scons_utils.copy_module_res_into_build(modname, "share", "share", must_exist=False, is_dry_run=is_dry_run)
    if builder.build_demo:
        scons_utils.copy_module_res_into_build(modname, "demo/etc", "etc", must_exist=False, is_dry_run=is_dry_run)
        scons_utils.copy_module_res_into_build(modname, "demo/include", "include", must_exist=False, is_dry_run=is_dry_run)
        scons_utils.copy_module_res_into_build(modname, "demo/share", "share", must_exist=False, is_dry_run=is_dry_run)
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
        builder.debug(f"Final configuration for module {modname}")
        pp.pprint(conf)
    if verbose and len(modules_build_order) > 1:
        print("")

if verbose:
    builder.debug(f"total targets registered: {len(targets)}")

###############################################################################
# Scons progress build output
###############################################################################

# scons_utils.build_add_progress_bar(targets)

progress_bar_fun = scons_utils.get_progress_bar_function(targets)
if progress_bar_fun:
  Progress(progress_bar_fun, interval = 1)


###############################################################################
# Scons after build
###############################################################################

import atexit

def after_build():
    success, failures_message = scons_utils.build_status()
    scons_utils.build_print_status(success, failures_message, build_start_time)
    if is_dry_run:
        return
    if success and hasattr(app, "on_build_success"):
        scons_utils.build_print_built(modules_generated_bins, modules_generated_demos, modules_generated_tests)
        app.on_build_success(build_modules, builder)
    elif hasattr(app, "on_build_fail"):
        app.on_build_fail(build_modules, builder)
    builder.symlink_build()
    builder.copy_dll_to_build(modules_build_order)
    if success and distribution:
        builder.distribute_app(app, build_modules)
    if compile_commands:
        builder.safe_symlink(compilation_db_path, os.path.join(builder.build_entry_path, "compile_commands.json"))


atexit.register(after_build)

builder.info("starting scons build (took {:.3f} sec)".format(time.time() - build_start_time))
