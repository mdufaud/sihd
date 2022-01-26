from conans import ConanFile
import os
import sys
import atexit
import inspect

from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

sys.dont_write_bytecode = True
import app
from _build_tools import modules as modules_helper
from _build_tools import builder as builder_helper

builder_helper.info("fetching external libraries for {}".format(app.name))

if builder_helper.verify_args() == False:
    exit(1)

builder_helper.sanitize_app(app)

modules_to_build = builder_helper.get_modules()
verbose = builder_helper.build_verbose
has_test = builder_helper.build_tests

if verbose:
    if modules_to_build:
        builder_helper.debug("getting libs from modules -> {}".format(modules_to_build))
    if has_test:
        builder_helper.debug("including test libs")

extlibs = {}

if modules_to_build != "NONE":
    builder_helper.info("parsing modules")

    modules = modules_helper.get_modules(app, specific_modules=modules_to_build)
    if verbose:
        builder_helper.debug("modules configuration: ")
        pp.pprint(modules)
        print()

    builder_helper.info("getting modules external libs")

    extlibs.update(modules_helper.get_modules_extlibs(app, modules))
    if has_test and hasattr(app, "test_libs"):
        extlibs.update(modules_helper.get_extlibs_versions(app, app.test_libs))

    if verbose:
        builder_helper.debug("modules external libs:")
        pp.pprint(extlibs)
        print()

builder_helper.info("looking for manual external libs to fetch")

more_libs = os.getenv("libs", "").split(",")
app_external_libs = hasattr(app, "extlibs") and app.extlibs or {}
for libname in more_libs:
    if not libname:
        continue
    if libname not in app_external_libs:
        builder_helper.warning("no external lib named: " + libname)
    else:
        builder_helper.info("added manual external lib: " + libname)
        extlibs[libname] = app_external_libs[libname]

builder_helper.info("setting up conan")
print()

post_treatment_libs = {
    "*lua.*": {"from": "lua.", "to": "lua5.3."}
}
current_filename = inspect.getframeinfo(inspect.currentframe()).filename
extlib_path = builder_helper.build_extlib_path
extlib_lib_path = builder_helper.build_extlib_lib_path
extlib_hdr_path = builder_helper.build_extlib_hdr_path
extlib_bin_path = builder_helper.build_extlib_bin_path
extlib_etc_path = builder_helper.build_extlib_etc_path
extlib_res_path = builder_helper.build_extlib_res_path

conan_options = {'*:shared': builder_helper.build_static_libs == False}
if hasattr(app, "conan_options"):
    conan_options.update(app.conan_options)

class ConanAppDependencies(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "gcc", "txt"
    default_options = conan_options

    def requirements(self):
        for name, version in extlibs.items():
            lib = "{}/{}".format(name, version)
            builder_helper.info("external lib to fetch: " + lib)
            self.requires(lib)
        print("")

    def imports(self):
        self.copy("*.*", dst = extlib_lib_path, src = "lib")
        self.copy("*.dll*", dst = extlib_lib_path, src = "bin") # transfert DLL to lib
        self.copy("*.*", dst = extlib_hdr_path, src = "include")
        self.copy("*", dst = extlib_bin_path, src = "bin")
        self.copy("*", dst = extlib_etc_path, src = "etc")
        self.copy("*", dst = extlib_res_path, src = "res")

def post_process():
    import glob
    for match, opt in post_treatment_libs.items():
        pattern = os.path.join(extlib_lib_path, match)
        results = glob.glob(pattern)
        builder_helper.info(pattern)
        builder_helper.info(*results)
        for result in results:
            link_path = result.replace(opt["from"], opt["to"])
            if os.path.exists(link_path):
                builder_helper.warning("removing symlink " + link_path)
                os.remove(link_path)
            builder_helper.info("adding symlink: {} -> {}".format(result, link_path))
            os.symlink(result, link_path)

atexit.register(post_process)