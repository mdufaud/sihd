"""
vcpkg dependency installer for SBT.

Resolves modules/extlibs, writes the vcpkg manifest, and dispatches
vcpkg install/list/tree commands.
Sub-modules handle triplet detection, cross-compilation, and manifest building.
"""

import sys
import os

# Ensure sbt root (site_scons/sbt/) is on sys.path for loader import
_sbt_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
if _sbt_dir not in sys.path:
    sys.path.insert(0, _sbt_dir)

from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

import loader

from site_scons.sbt.build import modules
from sbt import builder
from sbt import logger
from sbt import architectures
from site_scons.sbt.build import utils

from sbt.vcpkg.triplets import detect_triplet
from sbt.vcpkg.cross import build_overlay_triplet_with_flags, generate_pkgconfig_wrapper
from sbt.vcpkg.manifest import (
    build_vcpkg_manifest,
    write_vcpkg_manifest,
    build_cross_linux_foundation_manifest,
)

app = loader.load_app()

vcpkg_bin_path = os.getenv("VCPKG_PATH", None)
if vcpkg_bin_path is None:
    vcpkg_dir_path = os.path.join(builder.build_root_path, ".vcpkg")
    vcpkg_bin_path = os.path.join(vcpkg_dir_path, "vcpkg")
    if builder.is_msys():
        vcpkg_bin_path += ".exe"

vcpkg_build_path = os.path.join(builder.build_path, "vcpkg")
vcpkg_build_manifest_path = os.path.join(vcpkg_build_path, "vcpkg.json")

sbt_triplet_path = f"{builder.sbt_path}/vcpkg/triplets"

# Addon paths: project-specific additions live in addon/vcpkg/
sbt_vcpkg_addon_path = os.path.join(builder.build_root_path, "site_scons", "addon", "vcpkg")
sbt_vcpkg_addon_overlay_ports_path = os.path.join(sbt_vcpkg_addon_path, "overlay-ports")
addon_triplet_path = os.path.join(sbt_vcpkg_addon_path, "triplets")

if builder.verify_args(app) == False:
    exit(1)

modules_to_build = builder.get_modules()
modules_forced_to_build = builder.get_force_build_modules()

modules_lst = builder.get_modules_lst()
if modules_forced_to_build:
    modules_lst.extend(builder.get_force_build_modules_lst())

build_platform = builder.build_platform
verbose = builder.build_verbose

if verbose:
    if modules_lst:
        logger.debug("getting libs from modules -> {}".format(modules_lst))
    if builder.build_tests:
        logger.debug("including test libs")
    if builder.build_demo:
        logger.debug("including demo libs")

skip_libs = getattr(app, "extlibs_skip", [])
skip_libs.extend(getattr(app, f"extlibs_skip_{build_platform}", []))

extlibs = {}
build_modules = {}

if modules_to_build != "NONE":
    # Get modules configuration for this build
    try:
        build_modules = modules.build_modules_conf(app, specific_modules=modules_lst)
    except RuntimeError as e:
        logger.error(str(e))
        exit(1)

    # Checking module availability on platforms
    deleted_modules = modules.check_platform(build_modules, build_platform)
    for deleted_modules in deleted_modules:
        logger.warning("module '{}' cannot compile on platform: {}".format(deleted_modules, build_platform))

    # exit if no modules to build after deleted
    if not build_modules:
        exit(0)

    if verbose:
        logger.debug("modules configuration: ")
        pp.pprint(build_modules)
        print()

    # get external libraries from modules, tests and demo
    extlibs.update(modules.get_modules_extlibs(app, build_modules, build_platform))

    if builder.build_tests and hasattr(app, "test_extlibs"):
        extlibs.update(modules.get_extlibs_versions(app, app.test_extlibs))

    if builder.build_demo and hasattr(app, "demo_extlibs"):
        extlibs.update(modules.get_extlibs_versions(app, app.demo_extlibs))

    # Remove specifically skipped libraries
    for skip_lib in skip_libs:
        if skip_lib in extlibs:
            logger.warning(f"skipping library {skip_lib}")
            del extlibs[skip_lib]

    if verbose:
        logger.debug("modules external libs:")
        pp.pprint(extlibs)
        print()


# Auto-detect vcpkg triplet
vcpkg_triplet = detect_triplet(vcpkg_bin_path, sbt_triplet_path, addon_triplet_path)


###############################################################################
# Helpers
###############################################################################

# Link-time library names that imply X11/Wayland headers are needed at build time.
# app.py can extend this set via `display_libs = {"custom-lib", ...}`
_DISPLAY_LIBS = {"glfw", "glfw3", "SDL3", "GL", "GLEW", "X11", "wayland-client"}
_DISPLAY_LIBS |= set(getattr(app, "display_libs", []))


def _needs_display_libs() -> bool:
    """
    Deduce whether X11/Wayland foundation packages are needed by inspecting
    the resolved modules' link libraries and environment variables.

    Returns True when:
      - Any module declares a display-related lib (glfw, SDL3, GL, X11, etc.)
        in its cross/native/platform-specific libs keys, OR
      - The x11=1 or wayland=1 environment flags are set (util clipboard/screenshot).
    """
    # Check env flags (util x11=1 / wayland=1)
    if os.getenv("x11") == "1" or os.getenv("wayland") == "1":
        return True

    # Inspect every resolved module's libs under all platform-qualified keys
    lib_keys = [
        "libs",
        "linux-libs",
        "linux-cross-libs",
        "linux-native-libs",
        "cross-libs",
        "native-libs",
    ]
    for _name, conf in build_modules.items():
        for key in lib_keys:
            if _DISPLAY_LIBS & set(conf.get(key, [])):
                return True
    return False


def _foundation_packages_installed() -> bool:
    """
    Check whether X11/Wayland foundation packages are already present in vcpkg_installed.
    We probe for libX11 (X11 core) and libwayland-client (Wayland core) as sentinels.
    """
    installed_lib = os.path.join(vcpkg_build_path, "vcpkg_installed", vcpkg_triplet, "lib")
    for sentinel in ("libX11.so", "libwayland-client.so"):
        if not os.path.exists(os.path.join(installed_lib, sentinel)):
            return False
    return True


###############################################################################
# vcpkg commands
###############################################################################

def _check_vcpkg():
    if os.path.exists(vcpkg_bin_path) is False:
        logger.error(f"VCPKG path does not exist: {vcpkg_bin_path}")
        logger.error(f"deploy VCPKG with: make vcpkg deploy")
        raise RuntimeError("no VCPKG installed")


def _execute_vcpkg_install():
    _check_vcpkg()

    from time import time
    start_time = time()

    logger.info("fetching external libraries for {}".format(app.name))

    copy_env = os.environ.copy()

    # Cross-linux: create a pkg-config wrapper that isolates from host .pc files
    if builder.is_cross_building() and builder.build_platform == "linux":
        overlay_dir = os.path.join(vcpkg_build_path, "overlay-triplets")
        wrapper_path = generate_pkgconfig_wrapper(
            overlay_dir, vcpkg_triplet, vcpkg_build_path, vcpkg_bin_path
        )
        copy_env["PKG_CONFIG"] = wrapper_path

    args = [
        vcpkg_bin_path,
        "install",
        f"--triplet={vcpkg_triplet}",
        "--allow-unsupported",
    ]

    # Inject extra compiler flags via an overlay triplet (if defined in app.py)
    overlay_flags_dir = build_overlay_triplet_with_flags(
        app, vcpkg_triplet, vcpkg_build_path, vcpkg_bin_path, sbt_triplet_path, addon_triplet_path
    )
    if overlay_flags_dir:
        args.append(f"--overlay-triplets={overlay_flags_dir}")

    args.append(f"--overlay-triplets={sbt_triplet_path}")
    if os.path.isdir(addon_triplet_path):
        args.append(f"--overlay-triplets={addon_triplet_path}")

    # Custom overlay ports (e.g. libxcursor, libcap which need cross-compilation fixes)
    if os.path.isdir(sbt_vcpkg_addon_overlay_ports_path):
        args.append(f"--overlay-ports={sbt_vcpkg_addon_overlay_ports_path}")

    # force recompilation
    # if builder.build_platform == "web":
    #     args.append("--no-binarycaching")

    copy_env["VCPKG_BINARY_SOURCES"] = "clear;default,readwrite"

    if copy_env.get("VCPKG_DEFAULT_BINARY_CACHE", None) is None:
        vcpkg_archive_path = os.path.join(vcpkg_dir_path, "archives")
        copy_env["VCPKG_DEFAULT_BINARY_CACHE"] = vcpkg_archive_path
        if not os.path.isdir(vcpkg_archive_path):
            os.makedirs(vcpkg_archive_path)

    if builder.is_msys():
        args += ["--host-triplet=x64-mingw-dynamic"]

    if verbose:
        logger.debug(f"executing '{args}' in '{vcpkg_build_path}'")

    from subprocess import run as subprocess_run
    number_of_seconds_per_lib = 180
    proc = subprocess_run(args, cwd=vcpkg_build_path, timeout=(number_of_seconds_per_lib * len(extlibs)), env=copy_env)

    logger.info("fetched in {:.3f} seconds".format(time() - start_time))

    return proc.returncode


def _execute_vcpkg_depend_info():
    _check_vcpkg()
    args = [vcpkg_bin_path, "depend-info"] + list(extlibs.keys())
    args += [
        f"--triplet={vcpkg_triplet}",
        f"--overlay-triplets={sbt_triplet_path}",
        "--format=tree",
        "--max-recurse=-1"
    ]
    if os.path.isdir(addon_triplet_path):
        args.append(f"--overlay-triplets={addon_triplet_path}")
    if os.path.isdir(sbt_vcpkg_addon_overlay_ports_path):
        args.append(f"--overlay-ports={sbt_vcpkg_addon_overlay_ports_path}")
    if verbose:
        logger.debug(f"executing '{args}' in '{vcpkg_build_path}'")

    from subprocess import run as subprocess_run
    proc = subprocess_run(args, cwd=vcpkg_build_path)

    return proc.returncode


def _execute_vcpkg_list():
    _check_vcpkg()
    args = (
        vcpkg_bin_path,
        "list",
    )
    if verbose:
        logger.debug(f"executing '{args}' in '{vcpkg_build_path}'")

    from subprocess import run as subprocess_run
    proc = subprocess_run(args, cwd=vcpkg_build_path)

    return proc.returncode


def _link_to_extlibs():
    downloaded_path = os.path.join(vcpkg_build_path, "vcpkg_installed", vcpkg_triplet)
    if os.path.exists(downloaded_path):
        # remove existing link:
        if os.path.islink(builder.build_extlib_path):
            os.unlink(builder.build_extlib_path)
        if os.path.exists(builder.build_extlib_path):
            os.unlink(builder.build_extlib_path)
        builder.safe_symlink(downloaded_path, builder.build_extlib_path)


###############################################################################
# Cross-linux display foundation pre-install
###############################################################################

def _pre_install_display_foundation():
    """
    Cross-linux only: ensure X11/Wayland foundation packages are installed
    before the main vcpkg install.

    Ports like glfw3/sdl3 use cmake FindX11/FindWayland at configure time
    but don't declare vcpkg dependencies on them (they expect system packages).
    On cross builds there are no system X11/Wayland headers, so vcpkg must
    provide them. A two-phase install guarantees they exist before any port
    that needs them is configured.

    Phase 1 uses a reduced manifest (only X11/Wayland packages) so vcpkg
    installs them first. We skip it entirely when:
      - Not cross-building for linux
      - No resolved extlib actually needs display libs (no imgui/glfw3/sdl3)
      - The foundation packages are already present from a previous run
    """
    if not (builder.is_cross_building() and builder.build_platform == "linux"):
        return

    if not _needs_display_libs():
        if verbose:
            logger.debug("cross-linux: no module needs X11/Wayland, skipping foundation install")
        return

    cross_extlibs = getattr(app, "vcpkg_cross_linux_extlibs", {})
    if not cross_extlibs:
        return

    if _foundation_packages_installed():
        logger.info("cross-linux: X11/Wayland foundation already installed, skipping phase 1")
        return

    logger.info("cross-linux phase 1: installing X11/Wayland foundation packages")
    foundation_manifest = build_cross_linux_foundation_manifest(app)
    write_vcpkg_manifest(vcpkg_build_manifest_path, foundation_manifest)
    return_code = _execute_vcpkg_install()
    if return_code != 0:
        logger.error("cross-linux phase 1 failed")
        sys.exit(return_code)
    # Rewrite the full manifest for phase 2
    logger.info("cross-linux phase 2: installing all packages")
    write_vcpkg_manifest(vcpkg_build_manifest_path, build_vcpkg_manifest(app, extlibs, needs_display_libs=True))


###############################################################################
# Entry points
###############################################################################

def fetch():
    """Write manifest, pre-install display foundations if needed, then install all."""
    display = _needs_display_libs()
    write_vcpkg_manifest(vcpkg_build_manifest_path, build_vcpkg_manifest(app, extlibs, needs_display_libs=display))
    _pre_install_display_foundation()
    return_code = _execute_vcpkg_install()
    if return_code == 0:
        _link_to_extlibs()
    sys.exit(return_code)


def list_packages():
    """Write manifest and list installed packages."""
    display = _needs_display_libs()
    write_vcpkg_manifest(vcpkg_build_manifest_path, build_vcpkg_manifest(app, extlibs, needs_display_libs=display))
    _execute_vcpkg_list()


def depend_info():
    """Write manifest and show dependency tree."""
    display = _needs_display_libs()
    write_vcpkg_manifest(vcpkg_build_manifest_path, build_vcpkg_manifest(app, extlibs, needs_display_libs=display))
    _execute_vcpkg_depend_info()


if __name__ == "__main__":
    if "fetch" in sys.argv:
        fetch()
    elif "list" in sys.argv:
        list_packages()
    elif "tree" in sys.argv:
        depend_info()
