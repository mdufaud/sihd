from conans import ConanFile
from os.path import join
from os import getenv
from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)
import platform
import sys
sys.dont_write_bytecode = True
import app
import _build_tools.modules

print("Getting {} app libs".format(app.name))

modules_to_build = getenv("modules", "")
build_platform = getenv("platform", "").lower()
make_tests = getenv("test", "") == "1"
verbose = getenv("verbose", "") == "1"
use_clang = getenv("clang", "") == "1"

if build_platform != "" and build_platform not in ("win", "windows", "linux", "mac", "android", "ios"):
    raise RuntimeError("Platform " + build_platform + " is not supported")
if build_platform == "win":
    build_platform = "windows"

# Specific
conditionnals = []
if getenv('lua', "") == "1":
    conditionnals.extend(["lua", "luabin"])
if getenv('py', "") == "1":
    conditionnals.append("py")

if verbose:
    if modules_to_build:
        print("{}: modules -> {}".format(app.name, modules_to_build))
    if make_tests:
        print("{}: test mode".format(app.name))

modules = _build_tools.modules.build_modules(app,
            specific_modules=modules_to_build,
            conditionnals=conditionnals)
if verbose:
    print("{}: modules configuration".format(app.name))
    pp.pprint(modules)
    print()

libs = _build_tools.modules.build_libs_versions(app, modules, test=make_tests)
if verbose:
    print("{}: modules libs".format(app.name))
    pp.pprint(libs)
    print()

destination = join("..", "extlib")

class ConanAppDependencies(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = ["{}/{}".format(name, version) for name, version in libs.items()]
    generators = "gcc", "txt", "cmake"
    default_options = {'*:shared': True}

    def configure(self):
        self.settings.compiler = use_clang and "clang++" or "gcc"
        self.settings.compiler.libcxx = "libstdc++11"
        self.settings.compiler.version = '7'
        self.settings.build_type = "Release"
        if build_platform == "":
            return

    def imports(self):
        self.copy("*.h*", dst=join(destination, "include"), src="include") # From bin to bin
        self.copy("*.dll", dst=join(destination, "lib"), src="bin") # From lib to bin
        self.copy("*", dst=join(destination, "lib"), src="lib") # From lib to bin