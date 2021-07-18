from conans import ConanFile
from os.path import join
from os import getenv
from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

import sys
sys.dont_write_bytecode = True
import app
import _build_tools.modules

print("Getting {} app libs".format(app.name))

install_modules = getenv("modules") or ""
tests_active = bool(getenv("test"))

# specific
build_lua = getenv("lua")
build_py = getenv("py")
conditionnals = []
if build_lua:
    conditionnals.append("lua")
    conditionnals.append("luabin")
if build_py:
    conditionnals.append("py")

if install_modules:
    print("{}: modules -> {}".format(app.name, install_modules))
if tests_active:
    print("{}: test mode".format(app.name))

modules = _build_tools.modules.build_modules(app,
    specific_modules=install_modules,
    test=tests_active,
    conditionnals=conditionnals)
print("{}: modules configuration".format(app.name))
pp.pprint(modules)
print()

libs = _build_tools.modules.build_libs_versions(app, modules, test=tests_active)
print("{}: modules libs".format(app.name))
pp.pprint(libs)
print()

destination = join("..", "extlib")

class ConanAppDependencies(ConanFile):
   settings = "os", "compiler", "build_type", "arch"
   requires = ["{}/{}".format(name, version) for name, version in libs.items()]
   generators = "gcc", "txt", "cmake"
   default_options = {'*:shared': True}

   def imports(self):
      self.copy("*.h*", dst=join(destination, "include"), src="include") # From bin to bin
      self.copy("*.dll", dst=join(destination, "lib"), src="bin") # From lib to bin
      self.copy("*", dst=join(destination, "lib"), src="lib") # From lib to bin