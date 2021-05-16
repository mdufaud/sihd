from conans import ConanFile
from os.path import join
from os import getenv
from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

import sys
sys.dont_write_bytecode = True
import app
import build_utils.modules_util

print("Getting {} app libs".format(app.name))

single_module = getenv("module")
tests_active = bool(getenv("test"))

if single_module:
    print("{}: single module -> {}".format(app.name, single_module))
if tests_active:
    print("{}: test mode".format(app.name))

modules = build_utils.modules_util.build_modules(app, single_module=single_module, test=tests_active)
print("{}: modules configuration".format(app.name))
pp.pprint(modules)
print()

libs = build_utils.modules_util.build_libs_versions(app, modules, test=tests_active)
print("{}: modules libs".format(app.name))
pp.pprint(libs)
print()

destination = join("..", "extlib")

class ConanAppDependencies(ConanFile):
   settings = "os", "compiler", "build_type", "arch"
   requires = libs
   generators = "gcc", "txt"
   #default_options = {"poco:shared": True, "openssl:shared": True}
   default_options = {}

   def imports(self):
      self.copy("*.h", dst=join(destination, "include"), src="include") # From bin to bin
      self.copy("*.dll", dst=join(destination, "lib"), src="bin") # From lib to bin
      self.copy("*", dst=join(destination, "lib"), src="lib") # From lib to bin