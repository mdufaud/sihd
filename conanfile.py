from conans import ConanFile
import os
import sys
import atexit
import inspect

from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

sys.dont_write_bytecode = True
import app
import _build_tools.modules
import _build_tools.builder
from _build_tools.builder import debug, info, warning

info("fetching {} external libraries".format(app.name))

_build_tools.builder.sanitize_app(app)

modules_to_build = _build_tools.builder.get_modules()
verbose = _build_tools.builder.has_verbose()
has_test = _build_tools.builder.has_test()
compiler = _build_tools.builder.get_compiler()
build_platform = _build_tools.builder.get_platform()
compile_mode = _build_tools.builder.get_compile_mode()

if verbose:
    if modules_to_build:
        debug("getting libs from modules -> {}".format(modules_to_build))
    if has_test:
        debug("including test libs")

modules = _build_tools.modules.get_modules(app, specific_modules=modules_to_build)
if verbose:
    debug("modules configuration: ")
    pp.pprint(modules)
    print()

extlibs = _build_tools.modules.get_modules_extlibs(app, modules)
if has_test and hasattr(app, "test_libs"):
    extlibs += _build_tools.modules.get_extlibs_versions(app, app.test_libs)
if verbose:
    debug("modules external libs:")
    pp.pprint(extlibs)
    print()

post_treatment_libs = {
    "*lua.*": {"from": "lua.", "to": "lua5.3."}
}
current_filename = inspect.getframeinfo(inspect.currentframe()).filename
current_dir = os.path.dirname(os.path.abspath(current_filename))

class ConanAppDependencies(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "gcc", "txt"
    default_options = {'*:shared': True}

    def requirements(self):
        for name, version in extlibs.items():
            lib = "{}/{}".format(name, version)
            info("external lib to fetch: " + lib)
            self.requires(lib)
        print("")

    def imports(self):
        extlib_path = os.path.join("..", "extlib")
        extlib_lib_path = os.path.join(extlib_path, "lib")
        self.copy("*.so*", dst = extlib_lib_path, src = "lib")
        self.copy("*.dylib*", dst = extlib_lib_path, src = "lib")
        self.copy("*.dll*", dst = extlib_lib_path, src = "bin") # transfert DLL to lib
        self.copy("*.h*", dst = os.path.join(extlib_path, "include"), src = "include")
        self.copy("*", dst = os.path.join(extlib_path, "bin"), src = "bin")

def post_process():
    import glob
    extlib_path = os.path.join(current_dir, "build", "extlib")
    extlib_lib_path = os.path.join(extlib_path, "lib")
    for match, opt in post_treatment_libs.items():
        pattern = os.path.join(extlib_lib_path, match)
        results = glob.glob(pattern)
        info(pattern)
        info(*results)
        for result in results:
            link_path = result.replace(opt["from"], opt["to"])
            if os.path.exists(link_path):
                warning("removing symlink " + link_path)
                os.remove(link_path)
            info("adding symlink: {} -> {}".format(result, link_path))
            os.symlink(result, link_path)

atexit.register(post_process)