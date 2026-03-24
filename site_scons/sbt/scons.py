from time import time
build_start_time = time()

# careful: Rewrites time module import
from SCons.Script import *

if GetOption("help"):
    Return()

# General utilities
from glob import glob
from os import environ
from os import path as os_path
from fileinput import input as file_input
from subprocess import call as subprocess_call
from shutil import which as shutil_which

# Pretty utility for verbose output
from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

###############################################################################
# App settings
###############################################################################

from sbt import loader
from sbt import builder
from sbt import logger
from sbt import architectures

from site_scons.sbt.build import modules
from site_scons.sbt.build import utils as build_utils

from site_scons.sbt.scons import utils as scons_utils
from site_scons.sbt.scons import android as scons_android

app = loader.load_app()

logger.info("building {}".format(app.name))
logger.info("building to: {}".format(builder.build_path))

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
bin_ext = build_platform == "web" and ".html" or (build_platform == "windows" and ".exe" or "")

compile_commands = build_utils.is_opt("compile_commands", "1")

if verbose:
    logger.info("modules: {}".format(modules_lst or "all"))
    logger.info("platform: " + build_platform)
    logger.info("compiler: " + builder.build_compiler_version)
    logger.info("machine: " + builder.build_machine)
    logger.info("mode: " + build_mode)
    logger.info("tests: " + (builder.build_tests and "yes" or "no"))
    logger.info("libraries: " + (builder.build_static_libs and "static" or "shared"))
    logger.info("address sanitizer: " + (builder.build_asan and "yes" or "no"))

# Get modules configuration for this build
try:
    build_modules = modules.build_modules_conf(app, specific_modules=modules_lst)
except RuntimeError as e:
    logger.error(str(e))
    Exit(1)

# Checking module availability on platforms
deleted_modules = modules.check_platform(build_modules, build_platform)
for deleted_modules in deleted_modules:
    logger.warning("module '{}' cannot compile on platform: {}".format(deleted_modules, build_platform))

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
            logger.warning("external library '{}' not declared in packet manager '{}'".format(missing, pkg_manager_name))
    except RuntimeError as e:
        logger.error(str(e))
        Exit(1)
    deps = set(packages.keys())
    if builder.build_tests:
        test_extlibs = modules.get_extlibs_versions(app, getattr(app, "test_extlibs", []))
        test_packages, missing_test_packages = modules.get_modules_packages(app, pkg_manager_name, test_extlibs)
        for missing in missing_test_packages:
            logger.warning("external library '{}' not declared in packet manager '{}'".format(missing, pkg_manager_name))
        deps.update(test_packages.keys())
    if builder.build_demo:
        demo_extlibs = modules.get_extlibs_versions(app, getattr(app, "demo_extlibs", []))
        demo_packages, missing_demo_packages = modules.get_modules_packages(app, pkg_manager_name, demo_extlibs)
        for missing in missing_demo_packages:
            logger.warning("external library '{}' not declared in packet manager '{}'".format(missing, pkg_manager_name))
        deps.update(demo_packages.keys())
    logger.info("dependencies:")
    print(" ".join(deps))
    Return()

###############################################################################
# Build environment
###############################################################################

global_libs = getattr(app, "libs", [])
global_platform_libs = getattr(app, "{}_libs".format(build_platform), [])
if verbose:
    logger.debug("modules configuration:")
    pp.pprint(build_modules)
    if global_libs:
        logger.debug("libs:")
        pp.pprint(global_libs)
    if global_platform_libs:
        logger.debug("{} libs:".format(build_platform))
        pp.pprint(global_platform_libs)
    print()

base_env = Environment(
    # binaries path
    ENV = environ,
    # c compile flags
    CFLAGS = getattr(app, 'c_flags', []),
    # c++ compile flags
    CXXFLAGS = getattr(app, 'cxx_flags', []),
    # c and c++ compile flags
    CPPFLAGS = getattr(app, 'flags', []),
    # extra #define for inside the code
    CPPDEFINES = getattr(app, 'defines', []),
    # link flags - explicitly add -L flags to ensure extlib paths are searched before system paths
    LINKFLAGS = [f"-L{builder.build_lib_path}", f"-L{builder.build_extlib_lib_path}"] + getattr(app, 'link', []),
    # link flags for binaries
    BIN_LINKFLAGS = getattr(app, 'bin_link', []),
    # headers path
    CPPPATH = [builder.build_extlib_hdr_path],
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

def add_defines_from_cli(env):
    """ Add defines from command line arguments -> def=NAME=VALUE """
    defines = []
    for key, value in ARGLIST:
        if key == "def":
            defines.append(value)
    env.Append(CPPDEFINES = defines)

add_defines_from_cli(base_env)

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
    and not compiler in ("mingw", "em", "ndk"):
    base_env.Append(
        RPATH = [
            os_path.abspath(builder.build_lib_path),
            os_path.abspath(builder.build_extlib_lib_path)
        ],
    )
    # Cross-compiling linkers (musl, aarch64...) don't resolve transitive shared
    # library dependencies via -L or -rpath. Adding -rpath-link lets the linker
    # find libs at link time.
    if builder.is_cross_building():
        base_env.Append(
            LINKFLAGS = [
                '-Wl,-rpath-link=' + os_path.abspath(builder.build_lib_path),
                '-Wl,-rpath-link=' + os_path.abspath(builder.build_extlib_lib_path),
            ],
        )

if build_platform == "windows":
    base_env.Append(
        LIBPATH = [builder.build_extlib_bin_path]
    )

###############################################################################
# Compiler settings
###############################################################################

compiler = builder.build_compiler

# GCC build
if compiler == "gcc":
    from site_scons.sbt.scons.compilers import gcc as compiler_gcc
    compiler_gcc.load_in_env(base_env)
# CLANG build
elif compiler == "clang":
    from site_scons.sbt.scons.compilers import clang as compiler_clang
    compiler_clang.load_in_env(base_env)
# MINGW build
elif compiler == "mingw":
    from site_scons.sbt.scons.compilers import mingw as compiler_mingw
    compiler_mingw.load_in_env(base_env)
# ZIG build
elif compiler == "zig":
    from site_scons.sbt.scons.compilers import zig as compiler_zig
    compiler_zig.load_in_env(base_env)
# EMSCRIPTEN build
elif compiler == "em":
    from site_scons.sbt.scons.compilers import emscripten as compiler_emscripten
    compiler_emscripten.load_in_env(base_env)
# ANDROID NDK build
elif compiler == "ndk":
    from site_scons.sbt.scons.compilers import ndk as compiler_ndk
    compiler_ndk.load_in_env(base_env)

ccache = ""
if not build_utils.is_opt("nocache"):
    ccache = shutil_which("ccache") and "ccache " or ""
    if verbose and ccache:
        logger.info("using ccache at: {}".format(shutil_which("ccache")))

    if not ccache:
        # Cache compile build with md5
        scons_cache_path = os_path.join(builder.build_root_path, '.scons_cache')
        if verbose:
            logger.info("using scons cache at: {}".format(scons_cache_path))
        CacheDir(scons_cache_path)

base_env.Prepend(
    CC = ccache,
    CXX = ccache,
    AR = ccache,
    RANLIB = ccache,
)

if compiler != "mingw" and compiler != "ndk" and not builder.is_msys():
    base_env.Replace(SHLIBVERSION = app.version)

###############################################################################
# General app configurations
###############################################################################

# add $platform-[flags/defines/link/libs]
# add $libtype-[flags/defines/link/libs]
# add $mode-[flags/defines/link/libs]
# add $compiler-[flags/defines/link/libs]
# add $platform--[flags/defines/link/libs]

if verbose:
    logger.debug(f"looking for app configurations:")

default_app_conf_to_get = (build_platform, libtype, build_mode, compiler, builder.libc)

def add_combination_app_conf_to_env(env):
    for idx, app_config in enumerate(default_app_conf_to_get, start=1):
        keys = [app_config]
        scons_utils.add_env_app_conf(app, env, *keys)
        for sub_app_config in default_app_conf_to_get[idx:]:
            keys.append(sub_app_config)
            scons_utils.add_env_app_conf(app, env, *keys)

add_combination_app_conf_to_env(base_env)

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
    compilation_db_path = os_path.join(builder.build_path, "compile_commands.json")
    try:
        base_env.Tool('compilation_db')
        # base_env["COMPILATIONDB_PATH_FILTER"] = f"*[!{builder.build_path}].*"
        base_env.CompilationDatabase(compilation_db_path)
    except Exception as e:
        logger.warning(e)
        pass

# Decides when to recompile - removing slow md5 in favor of timestamps
# Decider('timestamp-newer')
# Decider('MD5')
Decider('MD5-timestamp')

###############################################################################
# Build setup
###############################################################################

modules_generated_libs = {}
modules_generated_bins = {}
modules_generated_tests = {}
modules_generated_demos = {}
modules_exported_test_includes = {}
modules_exported_test_resources = {}
modules_exported_bmis = {}
modules_cpp_modules = {}
modules_cloned_git_repositories = {}
targets = []
# compiled object nodes per module for combined library
modules_lib_objects = {}

def add_generated(gentype, dic: dict, module_name, name, path):
    dic.setdefault(module_name, []).append({"name": name, "path": path})
    if verbose:
        logger.debug(f"module '{name}' registered {gentype}: {path}")

def add_targets(src):
    """ Remember every source file used to build """
    global targets
    if isinstance(src, str):
        targets.append(File(src))
    elif isinstance(src, list):
        targets.extend(src)
    else:
        targets.append(src)

def _normalize_module_relative_path(module_name, path):
    path = os_path.normpath(str(path))
    module_root = os_path.join(builder.build_root_path, module_name)
    module_obj_root = os_path.join(builder.build_obj_path, module_name)
    if os_path.isabs(path):
        if path.startswith(module_root + os_path.sep):
            return os_path.relpath(path, module_root)
        if path.startswith(module_obj_root + os_path.sep):
            return os_path.relpath(path, module_obj_root)
        return path
    if path.startswith(module_root + os_path.sep):
        return os_path.relpath(path, module_root)
    if path.startswith(module_obj_root + os_path.sep):
        return os_path.relpath(path, module_obj_root)
    return path

def _prepend_unique_paths(env, key, values):
    existing = {str(v) for v in env.get(key, [])}
    to_prepend = []
    for value in values:
        s = str(value)
        if s not in existing and s not in {str(v) for v in to_prepend}:
            to_prepend.append(value)
    if to_prepend:
        env.Prepend(**{key: to_prepend})

def _enable_cpp_modules(env):
    """Register .cppm and .ixx as compilable suffixes in *env* using GCC named-module support.

    Reuses the existing .cpp LazyAction — do NOT use the CompositeBuilder's
    CommandGeneratorAction (object_builder.action), it recurses infinitely.

    Passes -fmodule-mapper=<mapper_file> so GCC writes/reads BMIs from
    build_path/gcm.cache/ regardless of SCons' CWD (workspace root).
    The mapper file is a simple '<module-name> <bmi-path>' text file.
    It is written/updated each time a module is registered and always
    complete before GCC actually compiles anything (config phase vs build phase).
    """
    mapper_file = _write_cpp_module_mapper()
    cpp_suffixes = ['.cppm', '.ixx']
    for builder_name in ['Object', 'SharedObject']:
        object_builder = env['BUILDERS'][builder_name]
        cpp_action = object_builder.builder.action.generator['.cpp']
        cpp_emitter = object_builder.emitter['.cpp']
        for suffix in cpp_suffixes:
            object_builder.add_action(suffix, cpp_action)
            object_builder.add_emitter(suffix, cpp_emitter)
            if suffix not in object_builder.builder.src_suffix:
                object_builder.builder.src_suffix.append(suffix)
    env.AppendUnique(CXXFLAGS=['-fmodules', f'-fmodule-mapper={mapper_file}'])
    env.AppendUnique(CPPSUFFIXES=cpp_suffixes)

def _ensure_cpp_modules_cache_dir():
    # Lives inside build_path which already encodes machine/platform/libc/compiler/mode.
    cache_dir = os_path.join(builder.build_path, 'gcm.cache')
    if not is_dry_run:
        os.makedirs(cache_dir, exist_ok=True)
    return cache_dir

def _write_cpp_module_mapper():
    """Write/update the GCC module mapper file from the current modules registry.

    Format (from GCC mapper spec): '<module-name> <bmi-path>' one per line.
    The file is rewritten on each new module registration during the SCons
    configuration phase — always complete before compilation starts.
    """
    cache_dir = _ensure_cpp_modules_cache_dir()
    mapper_file = os_path.join(builder.build_path, 'gcm.cache.mapper')
    if is_dry_run:
        return mapper_file
    with open(mapper_file, 'w') as f:
        for module_name in modules_cpp_modules:
            bmi_path = os_path.join(cache_dir, f'{module_name}.gcm')
            f.write(f'{module_name} {bmi_path}\n')
    return mapper_file

def _parse_cpp_module_name(path):
    import re
    module_re = re.compile(r'^\s*(?:export\s+)?module\s+([A-Za-z_][A-Za-z0-9_.:.]*)\s*;')
    with open(path, 'r', encoding='utf-8') as fd:
        for line in fd:
            m = module_re.match(line)
            if m:
                return m.group(1)
    raise RuntimeError(f"unable to determine C++ module name from: {path}")

def _resolve_cpp_module_source_path(module_dir, source):
    source_path = str(source)
    if os_path.isabs(source_path):
        return source_path
    candidate = os_path.join(module_dir, source_path)
    return candidate if os_path.exists(candidate) else source_path

def _split_build_inputs(env, src):
    """Split *src* into (sources_to_compile, prebuilt_object_nodes)."""
    sources = []
    prebuilt = []
    object_suffixes = {env.subst('$OBJSUFFIX'), env.subst('$SHOBJSUFFIX')}
    for value in scons_utils.as_list(src):
        get_suffix = getattr(value, 'get_suffix', None)
        suffix = get_suffix() if get_suffix else os_path.splitext(str(value))[1]
        (prebuilt if suffix in object_suffixes else sources).append(value)
    return sources, prebuilt

def _resolve_cpp_module_nodes(imports):
    missing = []
    resolved = []
    for name in scons_utils.dedupe_keep_order(scons_utils.as_list(imports)):
        info = modules_cpp_modules.get(name)
        if info is None:
            missing.append(name)
        else:
            resolved.extend(info.get('objects', []))
    if missing:
        raise RuntimeError("missing exported C++ modules: {}".format(", ".join(missing)))
    return scons_utils.dedupe_keep_order(resolved)

def _extract_cpp_module_imports(kwargs):
    local_kwargs = kwargs.copy()
    imports = scons_utils.as_list(local_kwargs.pop('cpp_modules', []))
    return imports, local_kwargs

def _build_objects_for_target(env, src, shared=False, cpp_modules=[], **kwargs):
    """Compile sources and wire GCC module Depends() edges; pass prebuilt objects through."""
    sources, prebuilt = _split_build_inputs(env, src)
    objects = []
    if sources:
        builder_func = env.SharedObject if shared else env.Object
        objects = scons_utils.as_list(builder_func(sources, **kwargs))
        if cpp_modules:
            env.Depends(objects, _resolve_cpp_module_nodes(cpp_modules))
    return objects + prebuilt

## modules environment methods

def _env_copy_into_build(self, src, dst):
    module_name = self['APP_MODULE_NAME']
    scons_utils.copy_module_res_into_build(module_name, src, dst, is_dry_run = is_dry_run)

def _env_file_basename(self, path):
    return os_path.basename(os_path.splitext(str(path))[0])

def _android_build_apk(env, src, name, output_dir, add_libs, android_dir=None, **kwargs):
    """ Build an Android APK from the given source(s). """
    import os
    import shutil

    module_name = env['APP_MODULE_NAME']
    module_dir = env['APP_MODULE_DIR']

    apk_env = env.Clone()
    apk_env.Prepend(LIBS = add_libs)
    scons_utils.add_env_app_conf(app, apk_env, "demo")
    for app_conf in default_app_conf_to_get:
        scons_utils.add_env_app_conf(app, apk_env, "demo", app_conf)

    # Resolve override directory
    module_android_dir = None
    if android_dir:
        module_android_dir = os_path.join(module_dir, android_dir)

    # Load override config
    config = scons_android.load_override_config(module_android_dir) if module_android_dir else {
        "namespace": "sihd.android.terminal",
        "native_activity": False,
    }

    sanitized_name = name.replace("-", "_").lower()

    # Determine extra sources
    extra_srcs = []
    ndk_root = builder.get_ndk_root()
    if config["native_activity"]:
        # NativeActivity mode: add android_native_app_glue.c
        glue_src = os_path.join(ndk_root, "sources", "android", "native_app_glue", "android_native_app_glue.c")
        extra_srcs.append(glue_src)
        apk_env.Append(SHLINKFLAGS = [
            '-u', 'ANativeActivity_onCreate',
            '-Wl,--no-undefined',
            '-static-libstdc++',
        ])
    else:
        # Terminal mode: add terminal_bridge.cpp
        bridge_src = os_path.join(scons_android.TEMPLATE_DIR, "app", "src", "main", "cpp", "terminal_bridge.cpp")
        extra_srcs.append(bridge_src)
        apk_env.Append(SHLINKFLAGS = [
            '-Wl,--no-undefined',
            '-static-libstdc++',
        ])
        # Ensure android and log libs are linked
        for lib in ['android', 'log']:
            if lib not in apk_env.get('LIBS', []):
                apk_env.Append(LIBS = [lib])

    # Build SharedLibrary
    all_srcs = [src] if isinstance(src, str) else list(src)
    objects = apk_env.SharedObject(all_srcs)
    # Compile extra sources (from template/NDK dirs) into build area
    # to avoid creating .os files next to the original sources
    for extra_src in extra_srcs:
        basename = os_path.splitext(os_path.basename(extra_src))[0]
        obj_path = os_path.join(builder.build_path, "apk-obj", sanitized_name, basename)
        objects.extend(apk_env.SharedObject(obj_path, extra_src))
    so_path = os_path.join(builder.build_lib_path, sanitized_name)
    so_lib = apk_env.SharedLibrary(so_path, objects)

    # Stage Gradle project, copy .so, build APK via env.Command
    staging_dir = os_path.join(builder.build_path, "apk-staging", sanitized_name)
    abi = scons_android.get_abi()
    jnilibs_so = os_path.join(staging_dir, "app", "src", "main", "jniLibs", abi, f"lib{sanitized_name}.so")

    apk_output = os_path.join(output_dir, f"{name}.apk")

    # Collect Android permissions from module config
    module_conf = env['APP_MODULE_CONF']
    android_permissions = module_conf.get('android-permissions', [])

    def _stage_and_build_apk(target, source, env):
        scons_android.stage_gradle_project(staging_dir, name, module_android_dir, permissions=android_permissions)
        scons_android.copy_so_to_jnilibs(staging_dir, str(source[0]), sanitized_name)
        return scons_android.build_apk(staging_dir, str(target[0]))

    apk_cmd = apk_env.Command(apk_output, so_lib, _stage_and_build_apk)
    apk_env.AlwaysBuild(apk_cmd)

    add_targets(src)
    return apk_cmd

def _env_build_demo(self, src, name, add_libs = [], android_dir = None, **kwargs):
    """ Environment method to build unit test binary for a module """
    global modules_generated_demos
    if builder.build_demo == False:
        return None
    module_name = self['APP_MODULE_NAME']

    # if modules are specified, demo is built only if specified by user and not automatically by dependencies
    if modules_lst and module_name not in modules_lst:
        return None

    if build_platform == "android":
        demo = _android_build_apk(self, src, name, builder.build_demo_path, add_libs, android_dir, **kwargs)
        add_generated("demo", modules_generated_demos, module_name, f"{name}.apk", os_path.join(builder.build_demo_path, f"{name}.apk"))
        return demo

    demo_env = self.Clone()
    demo_env.Prepend(LIBS = add_libs)
    cpp_modules, build_kwargs = _extract_cpp_module_imports(kwargs)
    scons_utils.add_env_app_conf(app, demo_env, "demo")
    for app_conf in default_app_conf_to_get:
        scons_utils.add_env_app_conf(app, demo_env, "demo", app_conf)
    if cpp_modules:
        _enable_cpp_modules(demo_env)

    if self['BIN_LINKFLAGS']:
        demo_env.Append(LINKFLAGS = self['BIN_LINKFLAGS'])

    name += bin_ext
    demo_path = os_path.join(builder.build_demo_path, name)
    if cpp_modules:
        demo_objects = _build_objects_for_target(demo_env, src, cpp_modules = cpp_modules, **build_kwargs)
        demo = demo_env.Program(demo_path, demo_objects, **build_kwargs)
    else:
        demo = demo_env.Program(demo_path, src, **build_kwargs)

    add_targets(src)

    add_generated("demo", modules_generated_demos, module_name, name, demo_path)

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
    cpp_modules, build_kwargs = _extract_cpp_module_imports(kwargs)
    conf = self['APP_MODULE_CONF']
    exported_test_cpppaths = []
    for dep_modname in conf.get("depends", []):
        for include_path in modules_exported_test_includes.get(dep_modname, []):
            exported_test_cpppaths.append(os_path.join(builder.build_root_path, dep_modname, include_path))
        for resource_path in modules_exported_test_resources.get(dep_modname, []):
            resource_src = os_path.join(builder.build_root_path, dep_modname, resource_path)
            resource_dst = os_path.join("test", "resources", dep_modname)
            if os_path.isfile(resource_src):
                resource_dst = os_path.join(resource_dst, os_path.basename(resource_path))
            scons_utils.copy_module_res_into_build(dep_modname, resource_path, resource_dst, must_exist=False, is_dry_run=is_dry_run)
    _prepend_unique_paths(test_env, "CPPPATH", exported_test_cpppaths)
    scons_utils.add_env_app_conf(app, test_env, "test")
    for app_conf in default_app_conf_to_get:
        scons_utils.add_env_app_conf(app, test_env, "test", app_conf)
    if cpp_modules:
        _enable_cpp_modules(test_env)

    if name is None:
        name = self["APP_MODULE_FORMAT_NAME"]
    name += bin_ext
    test_path = os_path.join(builder.build_test_path, "bin", name)

    if self['BIN_LINKFLAGS']:
        test_env.Append(LINKFLAGS = self['BIN_LINKFLAGS'])

    if cpp_modules:
        test_objects = _build_objects_for_target(test_env, src, cpp_modules = cpp_modules, **build_kwargs)
        test = test_env.Program(test_path, test_objects, **build_kwargs)
    else:
        test = test_env.Program(test_path, src, **build_kwargs)

    add_targets(src)

    global modules_generated_tests
    add_generated("test", modules_generated_tests, module_name, name, test_path)

    return test

def _env_build_lib(self, src, name = None, static = None, **kwargs):
    """ Environment method to build a shared library for a module """
    module_name = self['APP_MODULE_NAME']

    if name is None:
        name = self["APP_MODULE_FORMAT_NAME"]
    lib_path = os_path.join(builder.build_lib_path, name)
    cpp_modules, build_kwargs = _extract_cpp_module_imports(kwargs)
    if cpp_modules:
        _enable_cpp_modules(self)

    # compile sources to objects first so they can be reused
    # by both the per-module library and the combined library
    if (static is not None and static) or builder.build_static_libs:
        objects = _build_objects_for_target(self, src, cpp_modules = cpp_modules, **build_kwargs)
        lib = NoCache(self.StaticLibrary(lib_path, objects, **build_kwargs))
    else:
        objects = _build_objects_for_target(self, src, shared = True, cpp_modules = cpp_modules, **build_kwargs)
        lib = NoCache(self.SharedLibrary(lib_path, objects, **build_kwargs))

    add_targets(src)

    global modules_generated_libs
    add_generated("lib", modules_generated_libs, module_name, name, lib_path)

    # store compiled objects for combined library
    if builder.build_combined:
        global modules_lib_objects
        modules_lib_objects.setdefault(module_name, []).extend(
            objects if isinstance(objects, list) else [objects]
        )

    return lib

def _env_export_test(self, includes = [], resources = []):
    module_name = self['APP_MODULE_NAME']
    if includes:
        normalized_includes = [_normalize_module_relative_path(module_name, path) for path in includes]
        modules_exported_test_includes.setdefault(module_name, [])
        for include_path in normalized_includes:
            if include_path not in modules_exported_test_includes[module_name]:
                modules_exported_test_includes[module_name].append(include_path)
    if resources:
        normalized_resources = [_normalize_module_relative_path(module_name, path) for path in resources]
        modules_exported_test_resources.setdefault(module_name, [])
        for resource_path in normalized_resources:
            if resource_path not in modules_exported_test_resources[module_name]:
                modules_exported_test_resources[module_name].append(resource_path)

def _env_build_cpp_modules(self, src, **kwargs):
    owner_module_name = self['APP_MODULE_NAME']
    if not builder.is_cpp_modules:
        raise RuntimeError("C++ modules are only supported with GCC 15+ on native Linux builds")
    local_kwargs = kwargs.copy()
    explicit_module_names = local_kwargs.pop('module_name', None)
    imports = scons_utils.as_list(local_kwargs.pop('imports', []))
    sources = scons_utils.as_list(src)
    module_dir = self['APP_MODULE_DIR']
    if explicit_module_names is None:
        module_names = [_parse_cpp_module_name(_resolve_cpp_module_source_path(module_dir, s)) for s in sources]
    else:
        module_names = scons_utils.as_list(explicit_module_names)
    if len(module_names) != len(sources):
        raise RuntimeError("module_name must match the number of C++ module sources")
    cppm_env = self.Clone()
    # Enable on the clone for compilation and on self so that other sources in
    # the same module env can link against the produced BMIs.
    _enable_cpp_modules(cppm_env)
    _enable_cpp_modules(self)
    cache_dir = _ensure_cpp_modules_cache_dir()
    objects = scons_utils.as_list(cppm_env.Object(sources, **local_kwargs))
    if imports:
        cppm_env.Depends(objects, _resolve_cpp_module_nodes(imports))
    exported = modules_exported_bmis.setdefault(owner_module_name, [])
    for module_name, object_node in zip(module_names, objects):
        if module_name not in exported:
            exported.append(module_name)
        modules_cpp_modules[module_name] = {
            'owner': owner_module_name,
            'cache_dir': cache_dir,
            'objects': [object_node],
        }
    # Rewrite the mapper file now that new modules are registered.
    # Modules that import these will call _enable_cpp_modules() later
    # (guarantee: SCons processes dependencies first), so the mapper file
    # will already contain these entries when those envs are configured.
    _write_cpp_module_mapper()
    add_targets(src)
    return objects

def _env_build_bin(self, src, name = None, add_libs = [], android_dir = None, **kwargs):
    """ Environment method to build a binary for a module """
    global modules_generated_bins
    module_name = self['APP_MODULE_NAME']

    if name is None:
        name = self['APP_MODULE_FORMAT_NAME']

    if build_platform == "android":
        apk = _android_build_apk(self, src, name, builder.build_bin_path, add_libs, android_dir, **kwargs)
        add_generated("bin", modules_generated_bins, module_name, f"{name}.apk", os_path.join(builder.build_bin_path, f"{name}.apk"))
        return apk

    bin_env = self.Clone()
    bin_env.Prepend(LIBS = add_libs)
    cpp_modules, build_kwargs = _extract_cpp_module_imports(kwargs)
    if cpp_modules:
        _enable_cpp_modules(bin_env)

    if self['BIN_LINKFLAGS']:
        bin_env.Append(LINKFLAGS = self['BIN_LINKFLAGS'])

    name += bin_ext
    bin_path = os_path.join(builder.build_bin_path, name)
    if cpp_modules:
        bin_objects = _build_objects_for_target(bin_env, src, cpp_modules = cpp_modules, **build_kwargs)
        bin = bin_env.Program(bin_path, bin_objects, **build_kwargs)
    else:
        bin = bin_env.Program(bin_path, src, **build_kwargs)

    add_targets(src)

    add_generated("bin", modules_generated_bins, module_name, name, bin_path)

    return bin

def _env_git_clone(self, url, branch, dest, recursive = False):
    global modules_cloned_git_repositories
    modname = self['APP_MODULE_NAME']
    if not os_path.isabs(dest):
        dest = os_path.join(builder.build_root_path, modname, dest)
    ret = True
    if os_path.isdir(dest):
        logger.info("repository already cloned: {}".format(dest))
    else:
        logger.info("cloning repository: {} -> {}".format(url, dest))
        args = ['git', 'clone']
        if recursive:
            args.append("--recursive")
        args.extend(['--branch', branch, '--depth', '1', url, dest])
        if verbose:
            logger.info("git clone cmd: '{}'".format(" ".join(args)))
        if not is_dry_run or builder.force_git_clone():
            ret = subprocess_call(args) == 0
    if ret:
        modules_cloned_git_repositories.setdefault(modname, []).append(dest)
    return ret

def _sed_replace(path, replace_dic):
    if not os_path.isfile(path):
        raise RuntimeError("File to replace {} does not exists".format(path))
    for key, value in replace_dic.items():
        subprocess_call(['sed', '-i', 's/{}/{}/g'.format(key, value), path])

def _fileinput_replace(path, replace_dic):
    if not os_path.isfile(path):
        raise RuntimeError("File to replace {} does not exists".format(path))
    for line in file_input(path, inplace=True):
        for key, value in replace_dic.items():
            print(line.replace(key, value), end='')

def _env_replace_in_file(self, path, dic):
    module_name = self['APP_MODULE_NAME']
    if not os_path.isabs(path):
        path = os_path.join(builder.build_root_path, module_name, path)
    logger.debug("replacing file: " + path)
    if not is_dry_run:
        _fileinput_replace(path, dic)

def _env_replace_in_build(self, to_replace, replace_dic):
    module_name = self['APP_MODULE_NAME']
    files_to_replace = []
    for pattern in to_replace:
        ret = glob(os_path.join(builder.build_path, module_name, pattern), recursive = True)
        if ret:
            files_to_replace += ret
        else:
            files_to_replace.append(pattern)
    if verbose:
        logger.debug("replacing values in build files - {} files to replace".format(len(files_to_replace)))
    for path in files_to_replace:
        logger.debug("replacing file: " + path)
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
    if not os_path.isabs(src):
        src = os_path.join(builder.build_root_path, module_name, src)
    with open(src, 'r') as fd:
        for line in fd:
            if str in line:
                return True
    return False

def _env_filter_files(self, files_lst, filter_lst):
    def _filter_fun(file):
        return os_path.basename(str(file)) not in filter_lst
    return list(filter(_filter_fun, files_lst))

# methods to build either libs, tests, demos or executables
base_env.AddMethod(_env_build_lib, "build_lib")
base_env.AddMethod(_env_build_bin, "build_bin")
base_env.AddMethod(_env_build_test, "build_test")
base_env.AddMethod(_env_build_demo, "build_demo")
base_env.AddMethod(_env_build_demos, "build_demos")
base_env.AddMethod(_env_export_test, "export_test")
base_env.AddMethod(_env_build_cpp_modules, "build_cpp_modules")
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
base_env.AddMethod(lambda self, *args, **kwargs: build_utils.is_opt(*args, **kwargs), "is_opt")
base_env.AddMethod(lambda self, *args, **kwargs: build_utils.get_opt(*args, **kwargs), "get_opt")
base_env.AddMethod(lambda self: self['APP_MODULE_NAME'], "module_name")
base_env.AddMethod(lambda self: self['APP_MODULE_FORMAT_NAME'], "module_format_name")
base_env.AddMethod(lambda self: self['APP_MODULE_CONF'], "module_conf")
base_env.AddMethod(lambda self: self['APP_MODULE_DIR'], "module_dir")
base_env.AddMethod(lambda self: self['APP_MODULES_BUILD'], "modules_to_build")
base_env.AddMethod(lambda self: app, "app")
base_env.AddMethod(lambda self: builder, "builder")
base_env.AddMethod(lambda self: logger, "logger")
base_env.AddMethod(lambda self: is_dry_run, "is_dry_run")

modules_options = [
    libtype,
    build_platform,
    f"mode-{build_mode}",
    compiler,
]

if builder.is_cross_building():
    modules_options.append("cross")
    modules_options.append(f"{build_platform}-cross")
else:
    modules_options.append("native")
    modules_options.append(f"{build_platform}-native")

def _add_lib_type_to_modules_options(modules_options):
    to_append = []
    for option in modules_options:
        if option != libtype:
            to_append.append(f"{option}-{libtype}")
    modules_options += to_append

_add_lib_type_to_modules_options(modules_options)

if verbose:
    logger.debug(f"will look for modules options:")
    pp.pprint([ [ f"{option}-{key}" for option in modules_options ] for key in ("libs", "flags", "link", "defines")])

def get_compilation_options(conf, key):
    ret = []
    for option in modules_options:
        val = conf.get(f"{option}-{key}", [])
        ret += val
    return ret

modules.resolve_modules_exports(build_modules, modules_options)

def create_module_env(conf, depends = [],
                        do_inherit_depends_defines = False,
                        do_inherit_depends_links = False,
                        do_inherit_depends_flags = False,
                        do_inherit_depends_generated_libs = False,
                    ):
    modname = conf["modname"]
    ordered_depends = depends[:]
    ordered_depends.sort(
        key = lambda dep_modname: len(build_modules.get(dep_modname, {}).get("depends", [])),
        reverse = True,
    )
    # Keep module-local libraries before transitive exports so static link order
    # remains correct on platforms like MinGW.
    libs = modules.get_module_libs(build_modules, modname)
    libs += get_compilation_options(conf, "libs")
    libs += conf.get("_resolved_export_libs", [])
    # filter out libs that don't exist with musl
    if builder.libc == "musl":
        libs = [lib for lib in libs if lib not in architectures.musl_excluded_libs]
    # add flag
    flags = conf.get("_resolved_export_flags", []) + conf.get("flags", [])
    flags += get_compilation_options(conf, "flags")
    # add linkflags
    link = conf.get("_resolved_export_link", []) + conf.get("link", [])
    link += get_compilation_options(conf, "link")
    # add defines
    defines = conf.get("_resolved_export_defines", []) + conf.get("defines", [])
    defines += get_compilation_options(conf, "defines")

    # add libs generated by parent modules
    depends_generated_libs = []
    depends_defines = []
    depends_links = []
    depends_flags = []
    for dep_modname in ordered_depends:
        if do_inherit_depends_generated_libs:
            for generated_lib_name in modules_generated_libs.get(dep_modname, []):
                if generated_lib_name["name"] not in depends_generated_libs:
                    depends_generated_libs.append(generated_lib_name["name"])
        depend_conf = build_modules.get(dep_modname, {})
        if depend_conf:
            if do_inherit_depends_defines:
                depends_defines.extend(depend_conf.get("defines", []))
            if do_inherit_depends_links:
                depends_links.extend(depend_conf.get("link", []))
            if do_inherit_depends_flags:
                depends_flags.extend(depend_conf.get("flags", []))
            if dep_modname in modules_exported_bmis:
                flags.append('-fmodules')

    # Create a specific environment for the module
    env = base_env.Clone()
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
        # adding headers path
        CPPPATH = [
            os_path.join(builder.build_root_path, modname, "include"),
        ],
    )
    if any(dep_modname in modules_exported_bmis for dep_modname in ordered_depends):
        _enable_cpp_modules(env)
    # module name
    env["APP_MODULE_NAME"] = modname
    # formatted module name PROJNAME_MODULENAME
    env["APP_MODULE_FORMAT_NAME"] = f"{app.name}_{modname}"
    # configuration of module for scons.py
    env["APP_MODULE_CONF"] = conf
    env["APP_MODULE_DIR"] = os_path.join(builder.build_root_path, modname)

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
    logger.info("building module: {}".format(modname))
    env = create_module_env(conf,
        depends = conf.get("depends", []),
        do_inherit_depends_defines = conf.get("inherit-depends-defines", False),
        do_inherit_depends_links = conf.get("inherit-depends-links", False),
        do_inherit_depends_flags = conf.get("inherit-depends-flags", False),
        # it is quite obvious you might want the libs generated from your dependencies
        do_inherit_depends_generated_libs = conf.get("inherit-depends-generated-libs", True),
    )
    # some helpful debug when building
    if verbose:
        logger.debug("needed libraries:")
        pp.pprint([ str(s) for s in env['LIBS'] ])
        logger.debug("needed headers:")
        pp.pprint([ str(s) for s in env['CPPPATH'] ])
        package_configs = conf.get("pkg-configs", [])
        if package_configs:
            logger.debug("needed packages configs")
            pp.pprint(package_configs)
        parse_configs = conf.get("parse-configs", [])
        if parse_configs:
            logger.debug("needed specific packages configs")
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
    built[modname] = SConscript(Dir(builder.build_root_path).Dir(modname).File("scons.py"),
                                variant_dir = os_path.join(builder.build_obj_path, modname),
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
        logger.debug(f"Final configuration for module {modname}")
        pp.pprint(conf)
    if verbose and len(modules_build_order) > 1:
        print("")

###############################################################################
# Combined library
###############################################################################

if builder.build_combined and modules_lib_objects:
    # collect all pre-compiled objects from all built modules
    combined_objects = []
    for conf in modules_build_order:
        modname = conf["modname"]
        combined_objects.extend(modules_lib_objects.get(modname, []))

    if combined_objects:
        # collect external libs (exclude our own generated libs)
        internal_lib_names = set()
        for libs_info_list in modules_generated_libs.values():
            for lib_info in libs_info_list:
                internal_lib_names.add(lib_info["name"])

        combined_external_libs = []
        seen_libs = set()
        for conf in modules_build_order:
            for lib in conf.get("libs", []):
                lib_str = str(lib)
                if lib_str not in seen_libs and lib_str not in internal_lib_names:
                    combined_external_libs.append(lib)
                    seen_libs.add(lib_str)

        combined_env = base_env.Clone()
        combined_env.Replace(LIBS = combined_external_libs)
        for conf in modules_build_order:
            combined_env.AppendUnique(
                CPPPATH = [os_path.join(builder.build_root_path, conf["modname"], "include")]
            )

        combined_lib_path = os_path.join(builder.build_lib_path, app.name)
        if builder.build_static_libs:
            NoCache(combined_env.StaticLibrary(combined_lib_path, combined_objects))
        else:
            NoCache(combined_env.SharedLibrary(combined_lib_path, combined_objects))

        logger.info("combined library target registered: {}".format(app.name))

if verbose:
    logger.debug(f"total targets registered: {len(targets)}")

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

def merge_built():
    merged = {}
    for d in [modules_generated_bins, modules_generated_tests, modules_generated_demos]:
        for key, value in d.items():
            if key in merged:
                merged[key].extend(value)
            else:
                merged[key] = value
    return merged

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
    if builder.build_platform == "windows" and not builder.build_static_libs:
        builder.copy_dll_to_build(merge_built())
    if success and distribution:
        builder.distribute_app(app, build_modules)
    if compile_commands:
        builder.safe_symlink(compilation_db_path, os_path.join(builder.build_entry_path, "compile_commands.json"))


atexit.register(after_build)

logger.info("starting scons build (took {:.3f} sec)".format(time.time() - build_start_time))
