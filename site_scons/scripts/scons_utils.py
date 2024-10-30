import time
import os
import sys
import shutil

# Pretty utility for verbosis
from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

###############################################################################
# Build helpers
###############################################################################

from . import builder

__env_to_key = {
    "LIBS": "_libs",
    "CFLAGS": "_c_flags",
    "CXXFLAGS": "_cxx_flags",
    "CPPFLAGS": "_flags",
    "CPPDEFINES": "_defines",
    "LINKFLAGS": "_link",
}

def add_env_app_conf(app, env, *keys):
    key = "_".join(keys)
    if builder.build_verbose:
        builder.debug(f"adding env app conf: {key}")
    for envkey, to_concat in __env_to_key.items():
        concat = key + to_concat
        attr = getattr(app, concat, None)
        if attr is not None:
            #prepends
            # env[envkey][:0] = attr
            #append
            if isinstance(attr, list):
                env[envkey].extend(attr)
            else:
                env[envkey].append(attr)

def load_env_packages_config(env, *configs):
    """ Parse multiple pkg-configs: libraries/includes utilities """
    libtype = "--static" if builder.is_static_libs else "--shared"
    return parse_config_command(env, [
        "pkg-config {} --cflags --libs {}".format(config, libtype)
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

def __do_copy(src, dst, *, follow_symlinks=True):
    do_copy = True
    if os.path.isfile(dst):
        src_mtime = os.path.getmtime(src)
        dst_mtime = os.path.getmtime(dst)
        if dst_mtime > src_mtime:
            do_copy = False
    if do_copy:
        return shutil.copy2(src, dst, follow_symlinks=follow_symlinks)
    return dst

def copy_module_res_into_build(module_name, src, dst, must_exist = True, is_dry_run = False):
    """ recursive copy of MODNAME/DIRNAME to build/DIRNAME """
    src = str(src)
    dst = str(dst)
    module_res = os.path.join(builder.build_root_path, module_name, src)
    build_output = os.path.join(builder.build_path, dst)
    if builder.has_verbose():
        builder.info("copying resources of module {}/{} -> build/{}".format(module_name, src, dst))
    if os.path.isfile(module_res):
        if not is_dry_run:
            __do_copy(module_res, build_output)
    elif os.path.isdir(module_res):
        if is_dry_run:
            return
        try:
            shutil.copytree(module_res, build_output, copy_function=__do_copy, dirs_exist_ok = True)
        except shutil.Error as exc:
            errors = exc.args[0]
            for error in errors:
                src, dst, msg = error
                builder.error(f"{src} - {dst} -> {msg}")
    elif must_exist:
        raise RuntimeError("for module {} resource {} not found".format(module_name, module_res))


###############################################################################
# Scons helpers
###############################################################################

__node_count = 0
__node_count_max = 0

def get_progress_bar_function(targets):
    global __node_count
    global __node_count_max

    try:
        screen = open('/dev/tty', 'w')
        __node_count = 0
        __node_count_max = len(targets)

        def progress_function(node):
            if node not in targets:
                return
            global __node_count
            __node_count += 1
            if __node_count_max > 0 :
                screen.write('\r[%3d%%] ' % (__node_count * 100 / __node_count_max))

        return progress_function
    except (OSError, IOError) as e:
        builder.error("won't display progress - reason: " + str(e))
    return None

def build_print_built(binaries, demos, tests):
    for modname, binpaths in binaries.items():
        builder.info("module {} binaries:".format(modname))
        for binpath in binpaths:
            builder.info("\t{}".format(binpath))

    for modname, demopaths in demos.items():
        builder.info("module {} demos:".format(modname))
        for demopath in demopaths:
            builder.info("\t{}".format(demopath))

    for modname, testpaths in tests.items():
        builder.info("module {} tests:".format(modname))
        for testpath in testpaths:
            builder.info("\t{}".format(testpath))

def __print_err(*args):
    print(*args, file=sys.stderr)

def build_print_status(success, failures_message, build_start_time):
    if not success:
        __print_err("==============================================================")
        builder.error("BUILD FAILED (took {:.3f} sec)".format(time.time() - build_start_time))
        builder.error(failures_message)
        __print_err("==============================================================")
    else:
        builder.info("build succeeded (took {:.3f} sec)".format(time.time() - build_start_time))

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
