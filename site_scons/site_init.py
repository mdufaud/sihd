# Time
import time
build_start_time = time.time()
import os
import sys
import shutil
import filecmp

# Pretty utility for verbosis
from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

###############################################################################
# Addons
###############################################################################

try:
    import addon.scripts
except ImportError:
    pass

###############################################################################
# Build helpers
###############################################################################

from scripts import builder

# scons options
is_dry_run = bool(GetOption("no_exec"))

verbose = builder.has_verbose()

__env_to_key = {
    "LIBS": "_libs",
    "CFLAGS": "_c_flags",
    "CXXFLAGS": "_cxx_flags",
    "CPPFLAGS": "_flags",
    "CPPDEFINES": "_defines",
    "LINKFLAGS": "_link",
}

def add_env_app_conf(app, env, key):
    for envkey, to_concat in __env_to_key.items():
        concat = key + to_concat
        attr = getattr(app, concat, None)
        if attr is not None:
            #prepends
            env[envkey][:0] = attr

def load_env_packages_config(env, *configs):
    """ Parse multiple pkg-configs: libraries/includes utilities """
    return parse_config_command(env, [
        "pkg-config {} --cflags --libs".format(config)
        for config in configs
    ])

def parse_config_command(env, *configs):
    """ Parse configs from binaries outputs """
    if builder.build_platform == "windows" or builder.build_compiler == "em":
        return False
    for config in configs:
        try:
            env.ParseConfig(config)
        except OSError as e:
            builder.warning("config '{}' not found".format(config))

def copy_module_res_into_build(module_name, src, dst, must_exist = True):
    """ recursive copy of MODNAME/DIRNAME to build/DIRNAME """
    src = str(src)
    dst = str(dst)
    module_res = os.path.join(builder.build_root_path, module_name, src)
    build_output = os.path.join(builder.build_path, dst)
    if builder.has_verbose():
        builder.info("copying resources of module {}/{} -> build/{}".format(module_name, src, dst))
    if os.path.isfile(module_res):
        compare_output_file = None
        if os.path.isfile(build_output):
            compare_output_file = build_output
        elif os.path.isdir(build_output):
            compare_output_file = os.path.join(build_output, os.path.basename(module_res))
        if compare_output_file and os.path.isfile(compare_output_file) and filecmp.cmp(module_res, compare_output_file):
            # file is same
            return
        if not is_dry_run:
            shutil.copy(module_res, build_output)
    elif os.path.isdir(module_res):
        if not is_dry_run:
            shutil.copytree(module_res, build_output, dirs_exist_ok = True)
    elif must_exist:
        raise RuntimeError("for module {} resource {} not found".format(module_name, module_res))

###############################################################################
# Scons helpers
###############################################################################

def build_failure_to_str(bf):
    """ Convert an element of GetBuildFailures() to a string in a useful way """
    import SCons.Errors
    if bf is None: # unknown targets product None in list
        return '(unknown tgt)'
    elif isinstance(bf, SCons.Errors.StopError):
        return str(bf)
    elif bf.node:
        return str(bf.node) + ': ' + bf.errstr
    elif bf.filename:
        return bf.filename + ': ' + bf.errstr
    return 'unknown failure: ' + bf.errstr

def build_status():
    """ Convert the build status to a 2-tuple, (status, msg) """
    from SCons.Script import GetBuildFailures
    build_failures = GetBuildFailures()
    if build_failures:
        # bf is normally a list of build failures; if an element is None,
        # it's because of a target that scons doesn't know anything about.
        success = False
        failures_message = "\n".join(["Failed building %s" % build_failure_to_str(x)
                                        for x in build_failures if x is not None])
    else:
        # if bf is None, the build completed successfully.
        success = True
        failures_message = ''
    return (success, failures_message)

###############################################################################
# Build utils
###############################################################################

def print_err(*args):
    print(*args, file=sys.stderr)