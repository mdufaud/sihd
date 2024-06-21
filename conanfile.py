import os
import sys
import atexit
import inspect

from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

sys.dont_write_bytecode = True
import app
from site_scons.scripts import modules as modules
from site_scons.scripts import builder as builder

builder.info("fetching external libraries for {}".format(app.name))

if builder.verify_args(app) == False:
    exit(1)

modules_to_build = builder.get_modules()
modules_forced_to_build = builder.get_force_build_modules()

modules_lst = builder.get_modules_lst()
if modules_forced_to_build:
    modules_lst.extend(builder.get_force_build_modules_lst())

build_platform = builder.build_platform
verbose = builder.build_verbose
has_test = builder.build_tests

if verbose:
    if modules_lst:
        builder.debug("getting libs from modules -> {}".format(modules_lst))
    if has_test:
        builder.debug("including test libs")

extlibs = {}

if modules_to_build != "NONE":
    builder.info("parsing modules")

    # Get modules configuration for this build
    try:
        build_modules = modules.build_modules_conf(app, specific_modules=modules_lst)
    except RuntimeError as e:
        builder.error(str(e))
        exit(1)

    # Checking module availability on platforms
    deleted_modules = modules.check_platform(build_modules, build_platform)
    for deleted_modules in deleted_modules:
        builder.warning("module '{}' cannot compile on platform: {}".format(deleted_modules, build_platform))

    if not build_modules:
        exit(0)

    if verbose:
        builder.debug("modules configuration: ")
        pp.pprint(build_modules)
        print()

    builder.info("getting modules external libs")

    extlibs.update(modules.get_modules_extlibs(app, build_modules, build_platform))
    if has_test and hasattr(app, "test_extlibs"):
        extlibs.update(modules.get_extlibs_versions(app, app.test_extlibs))

    if verbose:
        builder.debug("modules external libs:")
        pp.pprint(extlibs)
        print()

builder.info("looking for manual external libs to fetch")

more_libs = os.getenv("libs", "").split(",")
app_external_libs = getattr(app, "extlibs", {})
for libname in more_libs:
    if not libname:
        continue
    if libname not in app_external_libs:
        builder.warning("no external lib named: " + libname)
    else:
        builder.info("added manual external lib: " + libname)
        extlibs[libname] = app_external_libs[libname]

builder.info("setting up conan")
print()

extlib_path = builder.build_extlib_path
extlib_lib_path = builder.build_extlib_lib_path
extlib_hdr_path = builder.build_extlib_hdr_path
extlib_bin_path = builder.build_extlib_bin_path
extlib_etc_path = builder.build_extlib_etc_path
extlib_res_path = builder.build_extlib_res_path

conan_options = {
    '*:shared': builder.build_static_libs == False,
}
if hasattr(app, "conan_options"):
    conan_options.update(app.conan_options)

try:
    from conans import ConanFile
except ImportError:
    from conan import ConanFile
    from conan.tools.files import copy

class ConanAppDependencies(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = []
    default_options = conan_options

    def system_requirements(self):
        pass

    def requirements(self):
        skip_libs = getattr(app, "conan_skip", [])
        for name, version in extlibs.items():
            if name in skip_libs:
                if verbose:
                    builder.info("skipping lib: " + name)
                pass
            lib = "{}/{}".format(name, version)
            builder.info("external lib to fetch: " + lib)
            self.requires(lib)
        print("")

    def imports(self):
        self.copy("*.*", dst = extlib_lib_path, src = "lib")
        self.copy("*.dll*", dst = extlib_lib_path, src = "bin") # transfert DLL to lib
        self.copy("*.*", dst = extlib_hdr_path, src = "include")
        self.copy("*", dst = extlib_bin_path, src = "bin")
        self.copy("*", dst = extlib_etc_path, src = "etc")
        self.copy("*", dst = extlib_res_path, src = "res")

    def generate(self):
        for dep in self.dependencies.values():
            for include_dir in dep.cpp_info.includedirs:
                copy(self, "*", include_dir, extlib_hdr_path)
            for bin_dir in dep.cpp_info.bindirs:
                copy(self, "*", bin_dir, extlib_bin_path)
            for lib_dir in dep.cpp_info.libdirs:
                copy(self, "*", lib_dir, extlib_lib_path)
            for res_dir in dep.cpp_info.resdirs:
                copy(self, "*", res_dir, extlib_res_path)
            for libs in dep.cpp_info.libs:
                copy(self, "*", libs, extlib_lib_path)

def post_process():
    import glob
    post_treatment_libs = getattr(app, "conan_post_process", {})
    for match, opt in post_treatment_libs.items():
        pattern = os.path.join(extlib_lib_path, match)
        results = glob.glob(pattern)
        if results:
            builder.info(pattern)
            builder.info(*results)
        for result in results:
            link_path = result.replace(opt["from"], opt["to"])
            if os.path.exists(link_path):
                builder.warning("removing symlink " + link_path)
                os.remove(link_path)
            builder.info("adding symlink: {} -> {}".format(result, link_path))
            os.symlink(result, link_path)

atexit.register(post_process)