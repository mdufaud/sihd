import platform
import sys
import os
import glob
import shutil
import tarfile
import subprocess
import datetime

from os.path import join, dirname, abspath

# append to python path parent build dir
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), os.pardir))

from scripts import modules as build_tools_modules

# import default env configuration
try:
    from addon import default_build

    env_keys = [item for item in dir(default_build) if not item.startswith("__")]
    for env_key in env_keys:
        if os.getenv(env_key, "") == "":
            value = getattr(default_build, env_key)
            os.environ[env_key] = str(value)

except ImportError:
    pass

# remove python path manipulation
sys.path.pop()

default_mode = "debug"
default_compiler = os.getenv("COMPILER", "gcc")

###############################################################################
# Term colors
###############################################################################

_term_color_prefix = "\033["
class TermColors(object):

    def __init__(self):
        if os.getenv("TERM"):
            self.red = f"{_term_color_prefix}0;31m"
            self.green = f"{_term_color_prefix}0;32m"
            self.orange = f"{_term_color_prefix}0;33m"
            self.blue = f"{_term_color_prefix}0;34m"
            self.bold_red = f"{_term_color_prefix}1;31m"
            self.bold_green = f"{_term_color_prefix}1;32m"
            self.bold_orange = f"{_term_color_prefix}1;33m"
            self.bold_blue = f"{_term_color_prefix}1;34m"
            self.reset = f"{_term_color_prefix}0m"
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

###############################################################################
# Build log
###############################################################################

def __log(color, level, *msg, file=sys.stderr):
    datestr = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"{color}[{datestr}] builder [{level}]:", *msg, term_colors.reset, file=file)

def debug(*msg):
    __log(term_colors.blue, "debug", *msg)

def info(*msg):
    __log(term_colors.green, "info", *msg)

def warning(*msg):
    __log(term_colors.orange, "warning", *msg)

def error(*msg):
    __log(term_colors.red, "error", *msg, )

###############################################################################
# Utils
###############################################################################

def safe_symlink(src, dst):
    if not os.path.exists(src):
        return False
    if os.path.islink(dst):
        os.remove(dst)
    if not os.path.exists(dst):
        info(f"linking {src} -> {dst}")
        if is_msys():
            #shutil.copyfile(src, dst)
            shutil.copy(src, dst)
        else:
            os.symlink(src, dst, target_is_directory=os.path.isdir(src))
    return True

def get_host_architecture():
    arch = platform.architecture()[0]
    if "64" in arch:
        return "64"
    elif "32" in arch:
        return "32"

def __get_machine(machine):
    return {
        "aarch64": "arm64",
        "amd64": "x86_64",
    }.get(machine, machine)

def get_host_machine():
    return __get_machine(platform.machine().lower())

def get_architecture():
    machine_to_arch = {
        "arm": "32",
        "i386": "32"
    }.get(get_opt('machine', None), None)
    if machine_to_arch is not None:
        return machine_to_arch
    return get_opt('arch', get_host_architecture())

def get_machine():
    machine = get_opt('machine', get_host_machine())
    if get_opt('arch', None) == "32":
        machine = {
            "arm64": "arm",
        }.get(machine, machine)
    return __get_machine(machine)

###############################################################################
# OS settings
# from conans/client/tools/oss.py in https://github.com/conan-io
###############################################################################
# from conans/conan/tools/gnu/get_gnu_triplet.py in https://github.com/conan-io
def _build_gnu_triplet(machine, vendor):
    op_system = {
        "windows": "w64-mingw32",
        "linux": "linux-gnu",
        "darwin": "apple-darwin",
        "android": "linux-android",
        "macos": "apple-darwin",
        "ios": "apple-ios",
        "watchos": "apple-watchos",
        "tvos": "apple-tvos",
        # NOTE: it technically must be "asmjs-unknown-emscripten" or
        # "wasm32-unknown-emscripten", but it's not recognized by old config.sub versions
        "emscripten": "local-emscripten",
        "aix": "ibm-aix",
        "neutrino": "nto-qnx"
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
# Build settings
###############################################################################

def get_opt(argname, default_val=""):
    arg_to_find = argname + "="
    for arg in sys.argv:
        idx = arg.find(arg_to_find)
        if idx >= 0:
            return arg[idx + len(arg_to_find):]
    return os.getenv(argname, "") or default_val

def is_opt(argname, default_val=""):
    ret = get_opt(argname, default_val)
    return ret == "1" or ret.lower() == "true"

def is_termux():
    return "ANDROID_ARGUMENT" in os.environ

def is_msys():
    return "MSYSTEM" in os.environ

def is_msys_mingw():
    return is_msys() and "MINGW" in os.getenv("MSYSTEM")

def __get_platform():
    env = get_opt("platform", "")
    build_platform = (env or platform.system()).lower()
    if "win" in build_platform or is_msys():
        build_platform = "windows"
    return build_platform

def get_compiler():
    build_platform = __get_platform()
    compiler = get_opt("compiler", default_compiler)
    if is_termux():
        compiler = "clang"
    if build_platform == "windows" and not is_msys():
        compiler = "mingw"
    return compiler.lower()

def get_platform():
    compiler = get_compiler()
    build_platform = __get_platform()
    if compiler == "mingw":
        build_platform = "windows"
    return build_platform

def get_pkgdep():
    return get_opt("pkgdep", "")

def get_compile_mode():
    return get_opt("mode", default_mode).lower()

def has_verbose():
    return (is_opt("verbose") or is_opt("v"))

def has_test():
    return (is_opt("test") or is_opt("t"))

def has_demo():
    return is_opt("demo")

def is_address_sanatizer():
    return is_opt("asan")

def do_distribution():
    return get_opt("dist", "") != ""

def get_modules():
    return get_opt("modules", "") or get_opt("m", "")

def get_force_build_modules():
    return get_opt("fmod", "")

def __split_modules(lst):
    return [m for m in lst.split(',') if len(m) > 0]

def get_modules_lst():
    return __split_modules(get_modules())

def get_force_build_modules_lst():
    return __split_modules(get_force_build_modules())

def is_static_libs():
    return is_opt("static")

def force_git_clone():
    return is_opt("fgit")

###############################################################################
# Build initialisation
###############################################################################

# compilation
build_compiler = get_compiler()
host_architecture = get_host_architecture()
build_architecture = get_architecture()
host_machine = get_host_machine()
build_machine = get_machine()
is_cross_building = host_machine != build_machine
build_mode = get_compile_mode()
build_static_libs = is_static_libs()
build_asan = is_address_sanatizer()
build_tests = has_test()
build_demo = has_demo()
build_verbose = has_verbose()

# platform
build_platform = get_platform()
build_on_android = is_termux()
build_for_windows = build_platform == "windows"
build_for_linux = build_platform == "linux"

def get_gnu_triplet():
    return _build_gnu_triplet(build_machine, build_platform)

# path ROOT -> root/site_scons/builder.py
build_root_path = abspath(dirname(dirname(dirname(__file__))))

# distribution path next to root
build_dist_path = join(build_root_path, "dist")
# build dir path next to root
build_entry_path = join(build_root_path, "build")
# last build link path
build_last_link_path = join(build_entry_path, "last")
# build full path
build_path = join(build_entry_path, f"{build_platform}-{build_machine}-{build_architecture}", build_compiler, build_mode)

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

libs_type = "static" if is_static_libs() else "dynamic"

###############################################################################
# App settings sanatizer
###############################################################################

allowed_compilers = ("gcc", "clang", "em", "mingw")

def verify_args(app):
    global build_static_libs
    ret = True
    if is_msys() and not is_msys_mingw():
        error("msys2 supported for mingw64 only")
        ret = False
    # if build_compiler == "mingw":
    #     error("please use msys2 instead of mingw")
    #     ret = False
    if build_compiler not in allowed_compilers:
        error("compiler {} is not supported".format(build_compiler))
        ret = False
    if hasattr(app, "modes") and build_mode not in app.modes:
        error("mode {} unknown".format(build_mode))
        ret = False
    if build_compiler == "mingw":
        if build_asan:
            error("cannot use address sanitizer with mingw")
            ret = False
        if not build_static_libs:
            warning("not supported Mingw without static libs - switching to static libs")
            build_static_libs = True
    elif build_compiler == "em":
        if build_asan:
            error("cannot use address sanitizer with emscripten")
            ret = False
        if not build_static_libs:
            warning("not supported Emscripten without static libs - switching to static libs")
            build_static_libs = True
    return ret

###############################################################################
# Windows utils
###############################################################################

def _get_libs_from_bin(path):
    ret = []
    try:
        ldd_output = subprocess.check_output(['ldd', path], universal_newlines=True)
        for line in ldd_output.splitlines():
            if '=>' in line:
                parts = line.split('=>')
                if len(parts) > 1:
                    lib_path = parts[1].strip().split(' ')[0]
                    if os.path.isabs(lib_path) and os.path.exists(lib_path):
                        ret.append(lib_path)
    except Exception as e:
        pass
    return ret

def copy_dll_to_build(generated_lld_binaries):
    build_need_dll_path_lst = [build_bin_path]
    if build_demo:
        build_need_dll_path_lst.append(build_demo_path)

    do_copy = False
    for path in build_need_dll_path_lst:
        if os.path.isdir(path):
            do_copy = True
            break

    if not do_copy:
        return

    dll_search_path_lst = [
        build_extlib_bin_path,
        build_extlib_lib_path,
    ]

    dll_forced_path_lst = [
        build_lib_path
    ]

    if is_msys():
        dll_search_path_lst.append("/mingw64/bin")

    dll_lst = set()
    for module_name, binaries_conf in generated_lld_binaries.items():
        for bin_conf in binaries_conf:
            path = bin_conf["path"]
            libs_path = _get_libs_from_bin(path)
            dll_lst.update(libs_path)

    print(dll_lst)

    dll_found = dll_lst
    # for search_path in dll_search_path_lst:
    #     if not os.path.isdir(search_path):
    #         continue
    #     for dll_name in dll_lst:
    #         dll_found.extend(glob.glob(os.path.join(search_path, "*{}*.dll".format(dll_name))))
    #         dll_found.extend(glob.glob(os.path.join(search_path, dll_name)))

    for forced_path in dll_forced_path_lst:
        if not os.path.isdir(forced_path):
            continue
        dll_found.update(glob.glob(os.path.join(forced_path, "*.dll")))

    for build_dll_path in build_need_dll_path_lst:
        if not os.path.isdir(build_dll_path):
            continue
        for dll_from in dll_found:
            dll_to = os.path.join(build_dll_path, os.path.basename(dll_from))
            print(f"COPY {dll_from} ---> {dll_to}")
            if os.path.exists(dll_to) and not os.path.samefile(dll_from, dll_to):
                os.remove(dll_to)
            shutil.copyfile(dll_from, dll_to)

###############################################################################
# After build
###############################################################################

def symlink_build():
    safe_symlink(build_path, build_last_link_path)

###############################################################################
# TAR distribution
###############################################################################

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

###############################################################################
# APT distribution
###############################################################################

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

###############################################################################
# PACMAN distribution
###############################################################################

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
            'platform={platform} compiler={compiler} arch={arch} machine={machine} mode={mode} ')
        .format(
            modules = get_opt("modules"),
            asan = get_opt("asan", "0"),
            static = get_opt("static", "0"),
            platform = build_platform,
            compiler = build_compiler,
            arch = build_architecture,
            machine = build_machine,
            mode = build_mode,
        ))
        if hasattr(app, "additionnal_build_env"):
            fd.write(" ".join(["{}={}".format(k, get_opt(k, "0")) for k in app.additionnal_build_env]))
        fd.write('\n}\n')
        fd.write('\n')
        fd.write(('package() {{\n'
            '\tcd "${{srcdir}}/${{pkgname}}-${{pkgver}}"\n'
            '\tmake install INSTALL_DESTDIR="${{pkgdir}}" INSTALL_PREFIX="/usr" '
            'platform={platform} compiler={compiler} arch={arch} machine={machine} mode={mode}\n'
        '}}\n').format(
            platform = build_platform,
            compiler = build_compiler,
            arch = build_architecture,
            machine = build_machine,
            mode = build_mode,
        ))
    # change back to old directory
    os.chdir(old_cwd)

###############################################################################
# Distribution
###############################################################################

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
# Python package command line utils
###############################################################################

if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(0)
    if sys.argv[1] == "compiler":
        print(build_compiler)
    elif sys.argv[1] == "platform":
        print(build_platform)
    elif sys.argv[1] == "arch":
        print(build_architecture)
    elif sys.argv[1] == "machine":
        print(build_machine)
    elif sys.argv[1] == "mode":
        print(build_mode)
    elif sys.argv[1] == "android":
        print(build_on_android and "true" or "false")
    elif sys.argv[1] == "path":
        print(build_path)
    elif sys.argv[1] == "static":
        print(libs_type)
    elif sys.argv[1] == "triplet":
        print(get_gnu_triplet())
    elif sys.argv[1] == "all":
        print(" ".join([
            build_platform,
            build_machine,
            build_mode,
            build_architecture,
            build_compiler,
            get_gnu_triplet(),
            build_on_android and "true" or "false",
            build_path,
        ]))
