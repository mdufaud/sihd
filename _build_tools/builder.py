import platform
import sys
import os
import glob
import shutil
import tarfile
from os.path import join, dirname, abspath
sys.path.append(".")
sys.dont_write_bytecode = True
try:
    import modules as build_tools_modules
except ImportError:
    from . import modules as build_tools_modules

linux_only_libs = ['dl', 'ssh']
linux_only_extlibs = ['pybind11', 'bluetooth']

default_compiler = os.getenv("COMPILER", "gcc")
specific_platform_compilers = {
    "windows": "mingw"
}
specific_compilers_platform = {v: k for k, v in specific_platform_compilers.items()}

def debug(*msg):
    print("builder [debug]:", *msg)

def info(*msg):
    print("builder [info]:", *msg)

def warning(*msg):
    print("builder [warning]:", *msg)

def error(*msg):
    print("builder [error]:", *msg, file=sys.stderr)

# from conans/client/tools/oss.py in https://github.com/conan-io

architectures = {
    "ppc": "ppc32",
    "sparc64": "sparcv9",
    "aarch64": "armv8",
    "arm64": "armv8",
    "64": "x86_64",
    "86": "x86",
    "arm": "armv6",
    "sun4v": "sparc"
}

ek2_architectures = {
    "E1C+": "e2k-v4",  # Elbrus 1C+ and Elbrus 1CK
    "E2C+": "e2k-v2",  # Elbrus 2CM
    "E2C+DSP": "e2k-v2",  # Elbrus 2C+
    "E2C3": "e2k-v6",  # Elbrus 2C3
    "E2S": "e2k-v3",  # Elbrus 2S (aka Elbrus 4C)
    "E8C": "e2k-v4",  # Elbrus 8C and Elbrus 8C1
    "E8C2": "e2k-v5",  # Elbrus 8C2 (aka Elbrus 8CB)
    "E12C": "e2k-v6",  # Elbrus 12C
    "E16C": "e2k-v6",  # Elbrus 16C
    "E32C": "e2k-v7",  # Elbrus 32C
}

def get_solaris_arch():
    # under intel solaris, platform.machine()=='i86pc' so we need to handle
    # it early to suport 64-bit
    processor = platform.processor()
    kernel_bitness, elf = platform.architecture()
    if "sparc" in processor:
        return "sparcv9" if kernel_bitness == "64bit" else "sparc"
    elif "i386" in processor:
        return "x86_64" if kernel_bitness == "64bit" else "x86"

def get_arch():
    plat = __get_platform()
    if plat == "sunos":
        arch = get_solaris_arch()
    else:
        arch = platform.machine()
        if "ek2" in arch:
            arch = ek2_architectures[arch]
        elif arch in architectures:
            arch = architectures[arch]
    return os.getenv('arch', "") or arch

def is_android():
    return "ANDROID_ARGUMENT" in os.environ

def __get_platform():
    env = os.getenv('platform', "")
    build_platform = (env or platform.system()).lower()
    if "win" in build_platform:
        build_platform = "windows"
    return build_platform

def get_compiler():
    arch = get_arch()
    build_platform = __get_platform()
    backup_compiler = default_compiler
    if "arm" in arch:
        backup_compiler = "clang"
    backup_compiler = os.getenv('compiler', "") or backup_compiler
    return specific_platform_compilers.get(build_platform, backup_compiler).lower()

def get_platform():
    compiler = get_compiler()
    build_platform = __get_platform()
    return specific_compilers_platform.get(compiler, build_platform)

def get_compile_mode():
    return (os.getenv("mode", "") or "debug").lower()

def has_verbose():
    return bool(os.getenv("verbose", None))

def has_test():
    return bool(os.getenv("test", None))

def is_address_sanatizer():
    return bool(os.getenv("asan", None))

def do_distribution():
    return bool(os.getenv("dist", None))

def get_modules():
    return os.getenv('modules', "")

def is_static_libs():
    return bool(os.getenv("static", None))

def sanitize_app(app):
    platform = get_platform()
    if platform != "windows":
        return
    print()
    info("cross building for windows - changes to app's configuration may apply")
    # remove libs from app.libs if they are linux only
    if hasattr(app, "libs") and isinstance(app.libs, list):
        for linux_lib in linux_only_libs:
            if linux_lib in app.libs:
                info("global lib '{}' is removed from list".format(linux_lib))
                app.libs.remove(linux_lib)
    # remove libs from app.test_libs if they are linux only
    if hasattr(app, "test_libs") and isinstance(app.test_libs, list):
        for linux_lib in linux_only_libs:
            if linux_lib in app.test_libs:
                info("global test lib '{}' is removed from list".format(linux_lib))
                app.test_libs.remove(linux_lib)
    # remove external libs dependencies in app.libs_versions if they are linux only
    if hasattr(app, "libs_versions") and isinstance(app.libs_versions, dict):
        to_remove = [name for name in app.libs_versions.keys() if name in linux_only_extlibs]
        for remove in to_remove:
            info("external lib '{}' is removed from list".format(remove))
            del app.libs_versions[remove]
    # remove modules from list if they depend on a linux lib
    modules = build_tools_modules.get_module_merged_with_conditionnals(app)
    to_remove = set()
    # check if module depends on a linux lib
    for modname, conf in modules.items():
        use_extlibs = conf.get("use-extlibs", [])
        matches = [lib for lib in use_extlibs if lib in linux_only_extlibs]
        if matches:
            warning("module '{}' use linux libs: {}".format(modname, ', '.join(matches)))
            to_remove.add(modname)
        libs = conf.get("libs", [])
        matches = [lib for lib in libs if lib in linux_only_extlibs]
        if matches:
            warning("module '{}' use linux libs: {}".format(modname, ', '.join(matches)))
            to_remove.add(modname)
    # removing modules that depends on removed modules
    for modname, conf in modules.items():
        depends = conf.get("depends", [])
        if any(mod in to_remove for mod in depends):
            to_remove.add(modname)
    for remove in to_remove:
        info("module '{}' is removed from list".format(remove))
        build_tools_modules.remove_module(app, remove)
    print()

# compilation
build_compiler = get_compiler()
build_architecture = get_arch()
build_mode = get_compile_mode()
build_static_libs = is_static_libs()
build_asan = is_address_sanatizer()
build_tests = has_test()
build_verbose = has_verbose()

# platform
build_platform = get_platform()
build_on_android = is_android()
build_for_windows = build_platform == "windows"
build_for_linux = build_platform == "linux"

# path ROOT
build_root_path = abspath(dirname(dirname(__file__)))

# path DIST
build_dist_path = join(build_root_path, "dist")

# path BUILD
build_entry_path = join(build_root_path, "build")
build_path = join(build_entry_path, "{}-{}".format(build_platform, build_architecture), build_mode)

build_extlib_path = join(build_path, "extlib")
build_extlib_lib_path = join(build_extlib_path, "lib")
build_extlib_hdr_path = join(build_extlib_path, "include")
build_extlib_bin_path = join(build_extlib_path, "bin")
build_extlib_etc_path = join(build_extlib_path, "etc")
build_extlib_res_path = join(build_extlib_path, "res")

build_bin_path = join(build_path, "bin")
build_hdr_path = join(build_path, "include")
build_etc_path = join(build_path, "etc")
build_lib_path = join(build_path, "lib")
build_test_path = join(build_path, "test")
build_obj_path = join(build_path, "obj")

def verify_args():
    global build_static_libs
    ret = True
    if build_platform not in ("windows", "linux"):
        error("platform {} is not supported".format(build_platform))
        ret = False
    if build_compiler not in ("gcc", "clang", "mingw", "em"):
        error("compiler {} is not supported".format(build_compiler))
        ret = False
    if build_mode not in ("debug", "release"):
        error("mode {} unknown".format(build_mode))
        ret = False
    if build_compiler == "mingw":
        if build_asan:
            error("cannot use address sanitizer with mingw")
            ret = False
    elif build_compiler == "em":
        if not build_static_libs:
            warning("not supported Emscripten without static libs - switching to static libs")
            build_static_libs = True
        if build_asan:
            error("cannot use address sanitizer with emscripten")
            ret = False
    return ret

## Distribution

def copy_dll_to_bin():
    if not os.path.isdir(build_bin_path):
        return
    libs_path = []
    if os.path.isdir(build_extlib_lib_path):
        libs_path.extend(glob.glob(os.path.join(build_extlib_lib_path, "*.dll")))
    if os.path.isdir(build_lib_path):
        libs_path.extend(glob.glob(os.path.join(build_lib_path, "*.dll")))
    for lib_path in libs_path:
        info("Copying '" + lib_path + "' to bin")
        shutil.copyfile(lib_path, os.path.join(build_bin_path, os.path.basename(lib_path)))

def create_tar_package(app):
    os.makedirs(build_dist_path, exist_ok = True)
    tar_path = join(build_dist_path, "{}-{}.tar.gz".format(app.name, app.version))
    info("compressing build to: " + tar_path)
    with tarfile.open(tar_path, "w:gz") as tar:
        if os.path.isdir(build_hdr_path):
            tar.add(build_hdr_path, arcname = os.path.basename(build_hdr_path))
        if os.path.isdir(build_lib_path):
            tar.add(build_lib_path, arcname = os.path.basename(build_lib_path))
        if os.path.isdir(build_bin_path):
            tar.add(build_bin_path, arcname = os.path.basename(build_bin_path))
        if os.path.isdir(build_etc_path):
            tar.add(build_etc_path, arcname = os.path.basename(build_etc_path))
        # extlibs
        if os.path.isdir(build_extlib_hdr_path):
            tar.add(build_extlib_hdr_path, arcname = os.path.basename(build_hdr_path))
        if not build_for_windows and os.path.isdir(build_extlib_lib_path):
            tar.add(build_extlib_lib_path, arcname = os.path.basename(build_lib_path))
        if os.path.isdir(build_extlib_bin_path):
            tar.add(build_extlib_bin_path, arcname = os.path.basename(build_bin_path))

# package dh-make
def create_apt_package(app):
    apt_path = join(build_dist_path, "apt", "{}-{}".format(app.name, app.version))
    control_path = join(apt_path, "control")
    copyright_path = join(apt_path, "copyright")
    info("creating apt package: " + apt_path)
    os.makedirs(apt_path, exist_ok = True)
    with open(control_path, "w") as fd:
        fd.write("Source: {}\n".format(app.name))
        fd.write("Section: {}\n".format("libdevel"))
        fd.write("Priority: {}\n".format("optional"))
        fd.write("Maintainer: {}\n".format(app.maintainer))
        fd.write("Build-Depends: {}\n".format("debhelper (>= 7)"))
        fd.write("\n")
        fd.write("Package: {}\n".format(app.name))
        fd.write("Architecture: {}\n".format("any"))
        fd.write("Depends: {}\n".format("${shlibs:Depends}"))
        fd.write("Description: {}\n".format(app.description))

def create_pacman_package(app):
    pacman_path = join(build_dist_path, "pacman", "{}-{}".format(app.name, app.version))
    info("creating pacman package: " + pacman_path)
    os.makedirs(pacman_path, exist_ok = True)

def distribute_app(app):
    info("distributing app " + app.name)
    create_tar_package(app)
    create_apt_package(app)
    create_pacman_package(app)

if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(0)
    if sys.argv[1] == "compiler":
        print(build_compiler)
    elif sys.argv[1] == "platform":
        print(build_platform)
    elif sys.argv[1] == "arch":
        print(build_architecture)
    elif sys.argv[1] == "mode":
        print(build_mode)
    elif sys.argv[1] == "android":
        print(build_on_android and "true" or "false")
    elif sys.argv[1] == "all":
        lst = [build_platform, build_architecture, build_mode, build_compiler, build_on_android and "true" or "false"]
        print(" ".join(lst))