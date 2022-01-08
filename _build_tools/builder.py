from os import getenv, environ
import platform
import sys
sys.path.append(".")
sys.dont_write_bytecode = True
try:
    import modules as build_tools_modules
except ImportError:
    from . import modules as build_tools_modules

linux_libs = ['dl', 'ssh']
linux_extlibs = ['readline', 'pybind11', 'bluetooth']

default_compiler = "gcc"
specific_platform_compilers = {
    "windows": "mingw"
}

def is_android():
    return "ANDROID_ARGUMENT" in environ

def get_platform():
    env = getenv('platform', "")
    build_platform = (env or platform.system()).lower()
    if build_platform == "win":
        build_platform = "windows"
    return build_platform

def get_compiler():
    arch = get_arch()
    build_platform = get_platform()
    backup_compiler = default_compiler
    if "arm" in arch:
        backup_compiler = "clang"
    env = getenv('compiler', "")
    backup_compiler = (env or backup_compiler).lower()
    return specific_platform_compilers.get(build_platform, backup_compiler)

def get_compile_mode():
    return (getenv("mode", "") or "debug").lower()

def has_verbose():
    return bool(getenv("verbose", None))

def has_test():
    return bool(getenv("test", None))

def do_sanitize():
    return bool(getenv("sanitize", None))

def get_modules():
    return getenv('modules', "")

def debug(*msg):
    print("builder [debug]:", *msg)

def info(*msg):
    print("builder [info]:", *msg)

def warning(*msg):
    print("builder [warning]:", *msg)

def error(*msg):
    print("builder [error]:", *msg, file=sys.stderr)

def sanitize_app(app):
    compiler = get_compiler()
    if compiler == "mingw":
        info("cross building for windows - changes to app's configuration may apply")
        # remove libs from app.libs if they are linux only
        if hasattr(app, "libs") and isinstance(app.libs, list):
            for linux_lib in linux_libs:
                if linux_lib in app.libs:
                    info("global lib '{}' is removed from list".format(linux_lib))
                    app.libs.remove(linux_lib)
        # remove libs from app.test_libs if they are linux only
        if hasattr(app, "test_libs") and isinstance(app.test_libs, list):
            for linux_lib in linux_libs:
                if linux_lib in app.test_libs:
                    info("global test lib '{}' is removed from list".format(linux_lib))
                    app.test_libs.remove(linux_lib)
        # remove external libs dependencies in app.libs_versions if they are linux only
        if hasattr(app, "libs_versions") and isinstance(app.libs_versions, dict):
            to_remove = [name for name in app.libs_versions.keys() if name in linux_extlibs]
            for remove in to_remove:
                info("external lib '{}' is removed from list".format(remove))
                del app.libs_versions[remove]
        # remove modules from list if they depend on a linux lib
        modules = build_tools_modules.get_module_merged_with_conditionnals(app)
        to_remove = set()
        # check if module depends on a linux lib
        for modname, conf in modules.items():
            uselibs = conf.get("uselibs", [])
            matches = [lib for lib in uselibs if lib in linux_extlibs]
            if matches:
                warning("module '{}' use linux libs: {}".format(modname, ', '.join(matches)))
                to_remove.add(modname)
            libs = conf.get("libs", [])
            matches = [lib for lib in libs if lib in linux_extlibs]
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
    plat = get_platform()
    if plat == "sunos":
        arch = get_solaris_arch()
    else:
        arch = platform.machine()
        if "ek2" in arch:
            arch = ek2_architectures[arch]
        elif arch in architectures:
            arch = architectures[arch]
    return getenv('arch', "") or arch

if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(0)
    if sys.argv[1] == "compiler":
        print(get_compiler())
    elif sys.argv[1] == "platform":
        print(get_platform())
    elif sys.argv[1] == "arch":
        print(get_arch())
    elif sys.argv[1] == "mode":
        print(get_compile_mode())
    elif sys.argv[1] == "android":
        print(is_android() and "true" or "false")