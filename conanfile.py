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

builder_helper.info("fetching {} external libraries".format(app.name))

builder_helper.sanitize_app(app)

modules_to_build = builder_helper.get_modules()
verbose = builder_helper.has_verbose()
has_test = builder_helper.has_test()
compiler = builder_helper.build_compiler
build_platform = builder_helper.build_platform
compile_mode = builder_helper.build_mode

if verbose:
    if modules_to_build:
        builder_helper.debug("getting libs from modules -> {}".format(modules_to_build))
    if has_test:
        builder_helper.debug("including test libs")

modules = modules_helper.get_modules(app, specific_modules=modules_to_build)
if verbose:
    builder_helper.debug("modules configuration: ")
    pp.pprint(modules)
    print()

extlibs = modules_helper.get_modules_extlibs(app, modules)
if has_test and hasattr(app, "test_libs"):
    extlibs.update(modules_helper.get_extlibs_versions(app, app.test_libs))
if verbose:
    builder_helper.debug("modules external libs:")
    pp.pprint(extlibs)
    print()

post_treatment_libs = {
    "*lua.*": {"from": "lua.", "to": "lua5.3."}
}
current_filename = inspect.getframeinfo(inspect.currentframe()).filename
extlib_path = builder_helper.build_extlib_path
extlib_lib_path =  builder_helper.build_extlib_lib_path
extlib_hdr_path =  builder_helper.build_extlib_hdr_path
extlib_bin_path =  builder_helper.build_extlib_bin_path

class ConanAppDependencies(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "gcc", "txt"
    default_options = {'*:shared': True}

    def requirements(self):
        for name, version in extlibs.items():
            lib = "{}/{}".format(name, version)
            builder_helper.info("external lib to fetch: " + lib)
            self.requires(lib)
        print("")

    def imports(self):
        self.copy("*.so*", dst = extlib_lib_path, src = "lib")
        self.copy("*.a*", dst = extlib_lib_path, src = "lib")
        self.copy("*.dylib*", dst = extlib_lib_path, src = "lib")
        self.copy("*.dll*", dst = extlib_lib_path, src = "bin") # transfert DLL to lib
        self.copy("*.h*", dst = extlib_hdr_path, src = "include")
        self.copy("*", dst = extlib_bin_path, src = "bin")

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