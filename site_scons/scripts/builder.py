import platform
import sys
import os
import glob
import shutil
import tarfile
import subprocess
from os.path import join, dirname, abspath
sys.path.append(".")
try:
    import modules as build_tools_modules
except ImportError:
    from . import modules as build_tools_modules

default_mode = "debug"
default_compiler = os.getenv("COMPILER", "gcc")
specific_platform_compilers = {
    "windows": "mingw"
}
specific_compilers_platform = {v: k for k, v in specific_platform_compilers.items()}

_term_color_prefix = "\033["
class TermColors(object):

    def __init__(self):
        if os.getenv("TERM"):
            self.red = "{}0;31m".format(_term_color_prefix)
            self.green = "{}0;32m".format(_term_color_prefix)
            self.orange = "{}0;33m".format(_term_color_prefix)
            self.blue = "{}0;34m".format(_term_color_prefix)
            self.bold_red = "{}1;31m".format(_term_color_prefix)
            self.bold_green = "{}1;32m".format(_term_color_prefix)
            self.bold_orange = "{}1;33m".format(_term_color_prefix)
            self.bold_blue = "{}1;34m".format(_term_color_prefix)
            self.reset = "{}0m".format(_term_color_prefix)
        else:
            self.red = ""
            self.green = ""
            self.orange = ""
            self.blue = ""
            self.bold_red = ""
            self.bold_green = ""
            self.bold_orange = ""
            self.bold_blue = ""
            self.reset = ""

term_colors = TermColors()

def debug(*msg):
    print(term_colors.blue + "builder [debug]:", *msg, term_colors.reset)

def info(*msg):
    print(term_colors.green + "builder [info]:", *msg, term_colors.reset)

def warning(*msg):
    print(term_colors.orange + "builder [warning]:", *msg, term_colors.reset)

def error(*msg):
    print(term_colors.red + "builder [error]:", *msg, term_colors.reset, file=sys.stderr)

###############################################################################
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
    return get_opt('arch', arch)

# from conans/conan/tools/gnu/get_gnu_triplet.py in https://github.com/conan-io

def _build_gnu_triplet(machine, vendor):
    op_system = {
        "Windows": "w64-mingw32",
        "Linux": "linux-gnu",
        "Darwin": "apple-darwin",
        "Android": "linux-android",
        "Macos": "apple-darwin",
        "iOS": "apple-ios",
        "watchOS": "apple-watchos",
        "tvOS": "apple-tvos",
        # NOTE: it technically must be "asmjs-unknown-emscripten" or
        # "wasm32-unknown-emscripten", but it's not recognized by old config.sub versions
        "Emscripten": "local-emscripten",
        "AIX": "ibm-aix",
        "Neutrino": "nto-qnx"
    }.get(vendor, vendor)
    if vendor in ("linux", "android"):
        if "arm" in machine and "armv8" not in machine:
            op_system += "eabi"
        if (machine == "armv5hf" or machine == "armv7hf") and vendor == "linux":
            op_system += "hf"
        if machine == "armv8_32" and vendor == "linux":
            op_system += "_ilp32"
    return "{}-{}".format(machine, op_system)

###############################################################################

def get_opt(argname, default_val=""):
    arg_to_find = argname + "="
    for arg in sys.argv:
        idx = arg.find(arg_to_find)
        if idx >= 0:
            return arg[idx + len(arg_to_find):]
    return os.getenv(argname, "") or default_val

def is_android():
    return "ANDROID_ARGUMENT" in os.environ

def __get_platform():
    env = get_opt("platform", "")
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
    backup_compiler = get_opt("compiler", backup_compiler)
    return specific_platform_compilers.get(build_platform, backup_compiler).lower()

def get_platform():
    compiler = get_compiler()
    build_platform = __get_platform()
    return specific_compilers_platform.get(compiler, build_platform)

def get_pkgdep():
    return get_opt("pkgdep", "")

def get_compile_mode():
    return get_opt("mode", default_mode).lower()

def has_verbose():
    return (get_opt("verbose", "") or get_opt("v", "")) == "1"

def has_test():
    return (get_opt("test", "") or get_opt("t", "")) == "1"

def has_demo():
    return get_opt("demo", "") == "1"

def is_address_sanatizer():
    return get_opt("asan", "") == "1"

def do_distribution():
    return get_opt("dist", "") != ""

def get_modules():
    return get_opt("modules", "") or get_opt("m", "")

def get_force_build_modules():
    return get_opt("fmod", "")

def is_static_libs():
    return get_opt("static", "") == "1"

# compilation
build_compiler = get_compiler()
build_architecture = get_arch()
build_mode = get_compile_mode()
build_static_libs = is_static_libs()
build_asan = is_address_sanatizer()
build_tests = has_test()
build_demo = has_demo()
build_verbose = has_verbose()

# platform
build_platform = get_platform()
build_on_android = is_android()
build_for_windows = build_platform == "windows"
build_for_linux = build_platform == "linux"

def get_gnu_triplet():
    return _build_gnu_triplet(build_architecture, build_platform)

# path ROOT -> root/site_scons/scripts/builder.py
build_root_path = abspath(dirname(dirname(dirname(__file__))))

# path DIST
build_dist_path = join(build_root_path, "dist")

# path BUILD
build_entry_path = join(build_root_path, "build")
build_last_link_path = join(build_entry_path, "last")
build_path = join(build_entry_path, "{}-{}".format(build_platform, build_architecture), build_mode)

build_extlib_path = join(build_path, "extlib")
build_extlib_bin_path = join(build_extlib_path, "bin")
build_extlib_lib_path = join(build_extlib_path, "lib")
build_extlib_hdr_path = join(build_extlib_path, "include")
build_extlib_etc_path = join(build_extlib_path, "etc")
build_extlib_res_path = join(build_extlib_path, "res")

build_bin_path = join(build_path, "bin")
build_lib_path = join(build_path, "lib")
build_hdr_path = join(build_path, "include")
build_etc_path = join(build_path, "etc")
build_share_path = join(build_path, "share")
build_test_path = join(build_path, "test")
build_demo_path = join(build_path, "demo")
build_obj_path = join(build_path, "obj")

def verify_args(app):
    global build_static_libs
    ret = True
    if build_platform not in ("windows", "linux"):
        error("platform {} is not supported".format(build_platform))
        ret = False
    if build_compiler not in ("gcc", "clang", "mingw", "em"):
        error("compiler {} is not supported".format(build_compiler))
        ret = False
    if hasattr(app, "modes") and build_mode not in app.modes:
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

def copy_dll_to_bin():
    dll_paths = []
    if os.path.isdir(build_extlib_lib_path):
        dll_paths.extend(glob.glob(os.path.join(build_extlib_lib_path, "*.dll*")))
    if os.path.isdir(build_lib_path):
        dll_paths.extend(glob.glob(os.path.join(build_lib_path, "*.dll*")))
    if os.path.isdir(build_bin_path):
        for dll_path in dll_paths:
            dst = os.path.join(build_bin_path, os.path.basename(dll_path))
            if os.path.exists(dst):
                os.remove(dst)
            shutil.copyfile(dll_path, dst)
    if not build_demo:
        return
    if os.path.isdir(build_demo_path):
        for dll_path in dll_paths:
            dst = os.path.join(build_demo_path, os.path.basename(dll_path))
            if os.path.exists(dst):
                os.remove(dst)
            shutil.copyfile(dll_path, dst)

def finalize():
    if os.path.exists(build_last_link_path):
        os.remove(build_last_link_path)
    os.symlink(build_path, build_last_link_path)
    if build_for_windows:
        copy_dll_to_bin()

###############################################################################
## Distribution
###############################################################################

# TAR
def create_tar_package(app):
    os.makedirs(build_dist_path, exist_ok = True)
    tar_path = join(build_dist_path, "{}-{}.tar.gz".format(app.name, app.version))
    info("compressing build to: " + tar_path)
    with tarfile.open(tar_path, "w:gz") as tar:
        # include
        if os.path.isdir(build_hdr_path):
            tar.add(build_hdr_path, arcname = os.path.basename(build_hdr_path))
        # lib
        if os.path.isdir(build_lib_path):
            tar.add(build_lib_path, arcname = os.path.basename(build_lib_path))
        # bin
        if os.path.isdir(build_bin_path):
            tar.add(build_bin_path, arcname = os.path.basename(build_bin_path))
        # etc
        if os.path.isdir(build_etc_path):
            tar.add(build_etc_path, arcname = os.path.basename(build_etc_path))
        # share
        if os.path.isdir(build_share_path):
            tar.add(build_share_path, arcname = os.path.basename(build_share_path))
        # extlibs/include
        if os.path.isdir(build_extlib_hdr_path):
            tar.add(build_extlib_hdr_path, arcname = os.path.basename(build_hdr_path))
        # extlibs/lib
        if not build_for_windows and os.path.isdir(build_extlib_lib_path):
            tar.add(build_extlib_lib_path, arcname = os.path.basename(build_lib_path))
        # extlibs/bin
        if os.path.isdir(build_extlib_bin_path):
            tar.add(build_extlib_bin_path, arcname = os.path.basename(build_bin_path))

# APT
def create_apt_package(app, modules):
    try:
        modules_extlibs = build_tools_modules.get_modules_extlibs(app, modules, platform)
        dependencies, missing_dep = build_tools_modules.get_modules_packages(app, "apt", modules_extlibs)
    except SystemExit as err:
        error(err)
        exit(1)
    apt_path = join(build_dist_path, "apt", "{}-{}".format(app.name, app.version))
    debian_path = join(apt_path, "DEBIAN")
    control_path = join(debian_path, "control")
    copyright_path = join(debian_path, "copyright")
    rules_path = join(debian_path, "rules")
    preinst_script_path = join(debian_path, "preinst")
    postinst_script_path = join(debian_path, "postinst")
    prerm_script_path = join(debian_path, "prerm")
    postrm_script_path = join(debian_path, "postrm")
    apt_share_path = join(apt_path, "usr", "share")
    info("creating apt package: " + apt_path)
    # debian/control
    os.makedirs(debian_path, exist_ok = True)
    info("creating apt control file: {}".format(control_path))
    with open(control_path, "w") as fd:
        fd.write("Package: {}\n".format(app.name))
        priority = hasattr(app, "priority") and app.priority or "optional"
        fd.write("Priority: {}\n".format(priority))
        fd.write("Maintainer: {}\n".format(app.maintainers[0]))
        all_contributors = app.maintainers[1:] + getattr(app, "contributors", [])
        if all_contributors:
            fd.write("Uploaders: {}\n".format(", ".join(all_contributors)))
        if hasattr(app, "url"):
            fd.write("Homepage: {}\n".format(app.url))
        if hasattr(app, "section"):
            fd.write("Section: {}\n".format(app.section))
        fd.write("Architecture: {}\n".format(app.architecture))
        if hasattr(app, "multi_architecture"):
            fd.write("Multi-Arch: {}\n".format(app.multi_architecture))
        fd.write("Version: {}\n".format(app.version))
        if dependencies:
            # Depends: libname (>= version), other_libname (>= other_version)
            fd.write("Depends: {}\n".format(", ".join(["{} (>= {})".format(k, v) for k, v in dependencies.items()])))
        fd.write("Description: {}\n".format(app.description))
    # /usr/bin
    apt_binary_path = join(apt_path, "usr", "bin")
    if os.path.isdir(build_bin_path):
        os.makedirs(apt_binary_path, exist_ok = True)
        shutil.copytree(build_bin_path, apt_binary_path, dirs_exist_ok = True)
    # /usr/include
    apt_include_path = join(apt_path, "usr", "include")
    if os.path.isdir(build_hdr_path):
        os.makedirs(apt_include_path, exist_ok = True)
        shutil.copytree(build_hdr_path, apt_include_path, dirs_exist_ok = True)
    # /usr/share
    apt_share_path = join(apt_path, "usr", "share")
    if os.path.isdir(build_share_path):
        os.makedirs(apt_share_path, exist_ok = True)
        shutil.copytree(build_share_path, apt_share_path, dirs_exist_ok = True)
    # /usr/lib/machine-vendor-os
    apt_lib_path = join(apt_path, "usr", "lib", "$(DEB_HOST_MULTIARCH)")
    if os.path.isdir(build_lib_path):
        os.makedirs(apt_lib_path, exist_ok = True)
        shutil.copytree(build_lib_path, apt_lib_path, dirs_exist_ok = True)
    # /etc
    apt_etc_path = join(apt_path, "etc")
    if os.path.isdir(build_etc_path):
        os.makedirs(apt_etc_path, exist_ok = True)
        shutil.copytree(build_etc_path, apt_etc_path, dirs_exist_ok = True)
    # dpkg-deb to build package
    if shutil.which("dpkg-deb") is not None:
        subprocess.call(['dpkg-deb', '--build', apt_path])
    else:
        error("dpkg-deb is missing")

# PACMAN
def create_pacman_package(app, modules):
    try:
        modules_extlibs = build_tools_modules.get_modules_extlibs(app, modules, platform)
        dependencies, missing_dep = build_tools_modules.get_modules_packages(app, "pacman", modules_extlibs)
    except SystemExit as err:
        error(err)
        exit(1)
    pacman_path = join(build_dist_path, "pacman", "{}-{}".format(app.name, app.version))
    info("creating pacman package: " + pacman_path)
    os.makedirs(pacman_path, exist_ok = True)
    pkg_build_path = join(pacman_path, "PKGBUILD")
    # change directory to pacman path
    old_cwd = os.getcwd()
    os.chdir(pacman_path)
    info("creating pacman PKGBUILD: {}".format(pkg_build_path))
    # create PKGBUILD file
    with open(pkg_build_path, "w") as fd:
        for maintainer in app.maintainers:
            fd.write('# Maintainer: {}\n'.format(maintainer))
        for uploader in getattr(app, "contributors", []):
            fd.write("# Contributor: {}\n".format(uploader))
        fd.write('\n')
        fd.write('pkgname={}\n'.format(app.name))
        fd.write('pkgver={}\n'.format(app.version))
        fd.write('pkgrel={}\n'.format(1))
        fd.write('pkgdesc="{}"\n'.format(app.description))
        fd.write("arch=('{}')\n".format(app.architecture))
        if hasattr(app, "url"):
            fd.write('url="{}"\n'.format(app.url))
        if hasattr(app, "license"):
            fd.write("license=('{}')\n".format(app.license))
        if hasattr(app, "section"):
            fd.write("groups=('{}')\n".format(app.section))
        fd.write("makedepends=('git' 'make' 'scons')\n")
        if dependencies:
            # depends=('libname>=version' 'other_libname>=other_version')
            fd.write("depends=({})\n".format(" ".join(["'{}>={}'".format(k, v) for k, v in dependencies.items()])))
        if hasattr(app, "pacman_source"):
            fd.write('source=("{}")\n'.format(app.pacman_source))
        fd.write("sha512sums=('SKIP')\n\n")
        fd.write(('build() {{\n'
            '\tcd "${{srcdir}}/${{pkgname}}-${{pkgver}}"\n'
            '\tmake fclean\n'
            '\tmake modules={modules} asan={asan} static={static} '
            'platform={platform} compiler={compiler} arch={arch} mode={mode} ')
        .format(
            modules = get_opt("modules"),
            asan = get_opt("asan", "0"),
            static = get_opt("static", "0"),
            platform = build_platform,
            compiler = build_compiler,
            arch = build_architecture,
            mode = build_mode,
        ))
        if hasattr(app, "additionnal_build_env"):
            fd.write(" ".join(["{}={}".format(k, get_opt(k, "0")) for k in app.additionnal_build_env]))
        fd.write('\n}\n')
        fd.write('\n')
        fd.write(('package() {{\n'
            '\tcd "${{srcdir}}/${{pkgname}}-${{pkgver}}"\n'
            '\tmake install INSTALL_DESTDIR="${{pkgdir}}" INSTALL_PREFIX="/usr" '
            'platform={platform} compiler={compiler} arch={arch} mode={mode}\n'
        '}}\n').format(
            platform = build_platform,
            compiler = build_compiler,
            arch = build_architecture,
            mode = build_mode,
        ))
    # change back to old directory
    os.chdir(old_cwd)

def distribute_app(app, modules):
    dist_type = get_opt("dist", None)
    if dist_type is None:
        raise SystemExit("cannot distribute app without its type")
    info("creating {} distribution for app {}".format(dist_type, app.name))
    if dist_type == "tar":
        create_tar_package(app)
    elif dist_type == "apt":
        create_apt_package(app, modules)
    elif dist_type == "pacman":
        create_pacman_package(app, modules)
    else:
        raise SystemExit("cannot distribute app type: {}".format(dist_type))

###############################################################################

def print_all():
    print(" ".join([
        build_platform,
        build_architecture,
        build_mode,
        build_compiler,
        build_on_android and "true" or "false"
    ]))

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
        print_all()