from conans import ConanFile
from os.path import join
from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)
import platform
import sys
sys.dont_write_bytecode = True
import app
import _build_tools.modules
import _build_tools.builder
from _build_tools.builder import debug, info

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

extlibs = _build_tools.modules.get_modules_extlibs(app, modules, test=has_test)
if verbose:
    debug("modules external libs:")
    pp.pprint(extlibs)
    print()

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
        destination = join("..", "extlib")
        self.copy("*.h*", dst=join(destination, "include"), src="include")
        self.copy("*.so*", dst=join(destination, "lib"), src="lib")
        self.copy("*.dylib*", dst=join(destination, "lib"), src="lib")
        self.copy("*.dll*", dst=join(destination, "lib"), src="bin") # transfert DLL to lib
        self.copy("*", dst=join(destination, "bin"), src="bin")