import os
import sys
import json

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

print(json.dumps({
    "dependencies": [
        "fmt"
    ],
    "builtin-baseline": "7977f0a771e64e9811d32aa30d9a247e09c39b2e",
    "overrides": [
        {
            "name": "fmt",
            "version": "7.1.3"
        },
    ],
}))