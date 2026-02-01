import platform
import glob
import shutil
import tarfile
import subprocess
import os
from os.path import join, dirname, abspath

try:
    from sbt import loader
    from sbt import architectures
except ImportError:
    import loader
    import architectures

from site_scons.sbt.build import modules as sbt_modules
from site_scons.sbt.build import utils
from sbt import logger

loader.load_env()

###############################################################################
# Utils
###############################################################################

def safe_symlink(src, dst):
    if not os.path.exists(src):
        return False
    if os.path.islink(dst):
        os.remove(dst)
    if not os.path.exists(dst):
        logger.info(f"linking {src} -> {dst}")
        if is_msys():
            if os.path.isdir(src):
                shutil.copytree(src, dst)
            else:
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

def get_architecture():
    machine = utils.get_opt('machine', None)
    if machine is not None:
        return architectures.get_default_arch(machine)
    return utils.get_opt('arch', get_host_architecture())

def __get_machine(machine):
    return architectures.normalize_machine(machine)

def get_host_machine():
    return __get_machine(platform.machine().lower())

def get_machine():
    arch = utils.get_opt('arch', None)
    if arch is not None and arch not in ("32", "64"):
        raise SystemExit("unknown architecture: {}".format(arch))
    
    machine = utils.get_opt('machine', None)
    if machine is not None:
        if arch == "32" and "64" in machine:
            raise SystemExit("cannot use 32 bits architecture with 64 bits machine: {}".format(machine))
    else:
        machine = get_host_machine()

    if arch == "32":
        machine = architectures.arch_32bit_map.get(machine, machine)
        
    return __get_machine(machine)

###############################################################################
# OS settings
# from conans/client/tools/oss.py in https://github.com/conan-io
###############################################################################
# from conans/conan/tools/gnu/get_gnu_triplet.py in https://github.com/conan-io
def _build_gnu_triplet(machine, vendor, libc="gnu"):
    op_system = {
        "windows": "w64-mingw32",
        "linux": f"linux-{libc}",
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
        if machine in ("armv6", "armv6l", "armv7", "armv7l"):
            # Add 'eabi' for ARM architectures
            op_system += "eabi"
        if machine in ("armv5hf", "armv7hf") and vendor == "linux":
            # Add 'hf' for hard-float ABI
            op_system += "hf"
        if machine == "armv8_32" and vendor == "linux":
            op_system += "_ilp32"
    return "{}-{}".format(machine, op_system)

###############################################################################
# Build settings
###############################################################################

def is_termux():
    return "ANDROID_ARGUMENT" in os.environ

def is_msys():
    return "MSYSTEM" in os.environ

def is_msys_mingw():
    return is_msys() and "MINGW" in os.getenv("MSYSTEM")

def __get_platform():
    env = utils.get_opt("platform", "")
    build_platform = (env or platform.system()).lower()
    if "win" in build_platform or is_msys():
        build_platform = "windows"
    return build_platform

def get_compiler():
    build_platform = __get_platform()
    compiler = utils.get_opt("compiler", os.getenv("COMPILER", "gcc"))
    if is_termux():
        compiler = "clang"
    if build_platform == "windows" and not is_msys():
        compiler = "mingw"
    elif build_platform == "web":
        compiler = "em"
    return compiler.lower()

def get_platform():
    compiler = get_compiler()
    build_platform = __get_platform()
    if compiler == "mingw":
        build_platform = "windows"
    elif compiler == "em":
        build_platform = "web"
    return build_platform

def detect_package_manager():
    """Detect the package manager of the current Linux distribution"""
    # Check for package managers in order of preference/commonality
    pkg_managers = [
        ("apt", ["apt", "apt-get"]),           # Debian, Ubuntu, Mint
        ("pacman", ["pacman"]),                 # Arch, Manjaro
        ("dnf", ["dnf"]),                       # Fedora, RHEL 8+, CentOS 8+
        ("yum", ["yum"]),                       # CentOS, RHEL, Amazon Linux
        ("zypper", ["zypper"]),                 # openSUSE, SLES
        ("apk", ["apk"]),                       # Alpine
        ("emerge", ["emerge"]),                 # Gentoo
        ("xbps", ["xbps-install"]),             # Void Linux
        ("swupd", ["swupd"]),                   # Clear Linux
        ("nix", ["nix-env"]),                   # NixOS
    ]
    
    for pkg_name, binaries in pkg_managers:
        for binary in binaries:
            if shutil.which(binary) is not None:
                return pkg_name
    
    # Fallback: try to detect from system files
    if os.path.exists("/etc/debian_version"):
        return "apt"
    elif os.path.exists("/etc/arch-release"):
        return "pacman"
    elif os.path.exists("/etc/fedora-release"):
        return "dnf"
    elif os.path.exists("/etc/redhat-release"):
        return "yum"
    elif os.path.exists("/etc/SuSE-release") or os.path.exists("/etc/SUSE-brand"):
        return "zypper"
    elif os.path.exists("/etc/alpine-release"):
        return "apk"
    elif os.path.exists("/etc/gentoo-release"):
        return "emerge"
    
    return ""

def get_pkgdep():
    pkgmanager = utils.get_opt("pkgdep", "")
    if pkgmanager == "auto":
        pkgmanager = detect_package_manager()
        if pkgmanager:
            logger.info(f"Auto-detected package manager: {pkgmanager}")
    return pkgmanager.lower()

def get_compile_mode():
    return utils.get_opt("mode", "default").lower()

def has_verbose():
    return (utils.is_opt("verbose") or utils.is_opt("v"))

def has_test():
    return (utils.is_opt("test") or utils.is_opt("t"))

def has_demo():
    return utils.is_opt("demo")

def is_address_sanitizer():
    return utils.is_opt("asan")

def do_distribution():
    return utils.get_opt("dist", "") != ""

def get_modules():
    return utils.get_opt("modules", "") or utils.get_opt("m", "")

def get_force_build_modules():
    return utils.get_opt("fmod", "")

def __split_modules(lst):
    return [m for m in lst.split(',') if len(m) > 0]

def get_modules_lst():
    return __split_modules(get_modules())

def get_force_build_modules_lst():
    return __split_modules(get_force_build_modules())

def is_static_libs():
    return utils.is_opt("static")

def force_git_clone():
    return utils.is_opt("fgit")

def get_libc():
    libc = utils.get_opt("libc", "gnu").lower()
    if libc not in ("gnu", "musl"):
        raise SystemExit("unknown libc: {}".format(libc))
    return libc

def get_host_libc():
    """Detect the host system's libc (gnu or musl)"""
    # Try to check if the system uses musl
    try:
        import subprocess
        # Check if ldd is using musl
        result = subprocess.run(['ldd', '--version'], capture_output=True, text=True)
        if 'musl' in result.stdout.lower() or 'musl' in result.stderr.lower():
            return "musl"
    except:
        pass
    # Default to gnu for most systems
    return "gnu"

###############################################################################
# Build initialisation
###############################################################################

# compilation
libc = get_libc()
build_compiler = get_compiler()
host_libc = get_host_libc()
host_architecture = get_host_architecture()
build_architecture = get_architecture()
host_machine = get_host_machine()
build_machine = get_machine()
build_mode = get_compile_mode()
build_static_libs = is_static_libs()
build_asan = is_address_sanitizer()
build_tests = has_test()
build_demo = has_demo()
build_verbose = has_verbose()

# platform
build_platform = get_platform()
build_on_android = is_termux()
build_for_windows = build_platform == "windows"
build_for_linux = build_platform == "linux"

def get_gnu_triplet():
    return _build_gnu_triplet(build_machine, build_platform, libc)

sbt_path = abspath(dirname(__file__))

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
# App settings sanitizer
###############################################################################

def is_cross_building():
    return (host_machine != build_machine) or (host_libc != libc)

allowed_compilers = ("gcc", "clang", "em", "mingw", "zig")

def verify_args(app):
    global build_static_libs
    global libc
    ret = True
    if is_msys() and not is_msys_mingw():
        logger.error("msys2 supported for mingw64 only")
        ret = False
    if build_compiler not in allowed_compilers:
        logger.error("compiler {} is not supported".format(build_compiler))
        ret = False
    if hasattr(app, "modes") and build_mode not in app.modes:
        logger.error("mode {} unknown".format(build_mode))
        ret = False

    if build_compiler == "zig":
        if libc != "musl":
            logger.warning("gnu libc is not supported with zig - switching to musl")
            libc = "musl"

    if build_compiler == "mingw":
        if build_asan:
            logger.error("cannot use address sanitizer with mingw")
            ret = False
        if not build_static_libs:
            logger.warning("not supported Mingw without static libs - switching to static libs")
            build_static_libs = True
    elif build_compiler == "em":
        if build_asan:
            logger.error("cannot use address sanitizer with emscripten")
            ret = False
        if not build_static_libs:
            logger.warning("not supported Emscripten without static libs - switching to static libs")
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

    dll_found = set()
    for module_name, binaries_conf in generated_lld_binaries.items():
        for bin_conf in binaries_conf:
            path = bin_conf["path"]
            libs_path = _get_libs_from_bin(path)
            dll_found.update(libs_path)

    for forced_path in dll_forced_path_lst:
        if not os.path.isdir(forced_path):
            continue
        dll_found.update(glob.glob(os.path.join(forced_path, "*.dll")))

    if has_verbose():
        for dll in dll_found:
            logger.debug(f"Found expected DLL: {dll}")

    for build_dll_path in build_need_dll_path_lst:
        if not os.path.isdir(build_dll_path):
            continue
        if has_verbose():
            logger.debug(f"Copying DLLs found into: {build_dll_path}")
        for dll_from in dll_found:
            dll_to = os.path.join(build_dll_path, os.path.basename(dll_from))

            if os.path.exists(dll_to):
                if os.path.samefile(dll_from, dll_to):
                    continue
                else:
                    os.remove(dll_to)
            shutil.copyfile(dll_from, dll_to)

###############################################################################
# After build
###############################################################################

def symlink_build():
    if not is_msys():
        safe_symlink(build_path, build_last_link_path)

###############################################################################
# TAR distribution
###############################################################################

def create_tar_package(app):
    os.makedirs(build_dist_path, exist_ok = True)
    tar_path = join(build_dist_path, "{}-{}.tar.gz".format(app.name, app.version))
    logger.info("compressing build to: " + tar_path)
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
        modules_extlibs = sbt_modules.get_modules_extlibs(app, modules, platform)
        dependencies, missing_dep = sbt_modules.get_modules_packages(app, "apt", modules_extlibs)
    except SystemExit as err:
        logger.error(err)
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
    logger.info("creating apt package: " + apt_path)
    # debian/control
    os.makedirs(debian_path, exist_ok = True)
    logger.info("creating apt control file: {}".format(control_path))
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
        logger.error("dpkg-deb is missing")

###############################################################################
# PACMAN distribution
###############################################################################

def create_pacman_package(app, modules):
    try:
        modules_extlibs = sbt_modules.get_modules_extlibs(app, modules, platform)
        dependencies, missing_dep = sbt_modules.get_modules_packages(app, "pacman", modules_extlibs)
    except SystemExit as err:
        logger.error(err)
        exit(1)
    pacman_path = join(build_dist_path, "pacman", "{}-{}".format(app.name, app.version))
    logger.info("creating pacman package: " + pacman_path)
    os.makedirs(pacman_path, exist_ok = True)
    pkg_build_path = join(pacman_path, "PKGBUILD")
    # change directory to pacman path
    old_cwd = os.getcwd()
    os.chdir(pacman_path)
    logger.info("creating pacman PKGBUILD: {}".format(pkg_build_path))
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
            modules = utils.get_opt("modules"),
            asan = utils.get_opt("asan", "0"),
            static = utils.get_opt("static", "0"),
            platform = build_platform,
            compiler = build_compiler,
            arch = build_architecture,
            machine = build_machine,
            mode = build_mode,
        ))
        if hasattr(app, "additionnal_build_env"):
            fd.write(" ".join(["{}={}".format(k, utils.get_opt(k, "0")) for k in app.additionnal_build_env]))
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
    dist_type = utils.get_opt("dist", None)
    if dist_type is None:
        raise SystemExit("cannot distribute app without its type")
    logger.info("creating {} distribution for app {}".format(dist_type, app.name))
    if dist_type == "tar":
        create_tar_package(app)
    elif dist_type == "apt":
        create_apt_package(app, modules)
    elif dist_type == "pacman":
        create_pacman_package(app, modules)
    else:
        raise SystemExit("cannot distribute app type: {}".format(dist_type))