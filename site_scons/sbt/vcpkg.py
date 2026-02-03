import sys
from json import dump as json_dump
import os

from pprint import PrettyPrinter
pp = PrettyPrinter(indent=2)

import loader

from site_scons.sbt.build import modules
from sbt import builder
from sbt import logger
from sbt import architectures
from site_scons.sbt.build import utils

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

    # emove specifically skipped libraries
    for skip_lib in skip_libs:
        if skip_lib in extlibs:
            logger.warning(f"skipping library {skip_lib}")
            del extlibs[skip_lib]

    if verbose:
        logger.debug("modules external libs:")
        pp.pprint(extlibs)
        print()

def build_vcpkg_triplet():
    """
    Built-in Triplets:
        x64-android          x64-linux            x64-uwp              arm64-windows
        arm64-android        x64-windows          arm64-uwp            x64-osx
        x86-windows          arm64-osx            arm-neon-android     x64-windows-static

    Community Triplets:
        x64-xbox-xboxone      arm-android                   x64-osx-dynamic             loongarch32-linux-release
        x64-ios               arm-uwp-static-md             arm-uwp                     x86-ios
        loongarch64-linux     arm64-windows-static-release  x86-uwp-static-md           arm64-uwp-static-md
        arm64-windows-static  x86-android                   arm64-mingw-dynamic         arm64ec-windows
        arm-mingw-static      x64-uwp-static-md             s390x-linux-release         x86-windows-static-md
        ppc64le-linux-release arm64-linux                   riscv32-linux-release       x86-windows-v120
        arm64-ios             x64-windows-static-md-release armv6-android               x86-mingw-dynamic
        x86-windows-static    x64-windows-static-release    loongarch64-linux-release   x86-mingw-static
        arm64-osx-dynamic     wasm32-emscripten             x64-xbox-scarlett-static    x64-mingw-static
        x64-xbox-scarlett     x64-linux-release             arm-windows-static          x64-xbox-xboxone-static
        x64-linux-dynamic     x64-osx-release               arm64-ios-simulator-release riscv32-linux
        arm-linux             x64-freebsd                   arm-linux-release           ppc64le-linux
        arm64-ios-simulator   x86-uwp                       arm-ios                     x64-windows-static-md
        arm64-mingw-static    mips64-linux                  riscv64-linux-release       arm64-windows-static-md
        arm64-linux-release   x86-freebsd                   x64-windows-release         s390x-linux
        arm64-ios-release     x86-linux                     x64-openbsd                 arm-windows
        x64-mingw-dynamic     arm-mingw-dynamic             arm64-osx-release           loongarch32-linux
        riscv64-linux
    """
    vcpkg_triplet = utils.get_opt("triplet", None)

    if vcpkg_triplet is None:
        builtin_triplets = [
            "x64-android", "x64-linux", "x64-uwp", "arm64-windows",
            "arm64-android", "x64-windows", "arm64-uwp", "x64-osx",
            "x86-windows", "arm64-osx", "arm-neon-android", "x64-windows-static"
        ]
        community_triplets = [
            "x64-xbox-xboxone", "arm-android", "x64-osx-dynamic", "loongarch32-linux-release",
            "x64-ios", "arm-uwp-static-md", "arm-uwp", "x86-ios",
            "loongarch64-linux", "arm64-windows-static-release", "x86-uwp-static-md", "arm64-uwp-static-md",
            "arm64-windows-static", "x86-android", "arm64-mingw-dynamic", "arm64ec-windows",
            "arm-mingw-static", "x64-uwp-static-md", "s390x-linux-release", "x86-windows-static-md",
            "ppc64le-linux-release", "arm64-linux", "riscv32-linux-release", "x86-windows-v120",
            "arm64-ios", "x64-windows-static-md-release", "armv6-android", "x86-mingw-dynamic",
            "x86-windows-static", "x64-windows-static-release", "loongarch64-linux-release", "x86-mingw-static",
            "arm64-osx-dynamic", "wasm32-emscripten", "x64-xbox-scarlett-static", "x64-mingw-static",
            "x64-xbox-scarlett", "x64-linux-release", "arm-windows-static", "x64-xbox-xboxone-static",
            "x64-linux-dynamic", "x64-osx-release", "arm64-ios-simulator-release", "riscv32-linux",
            "arm-linux", "x64-freebsd", "arm-linux-release", "ppc64le-linux",
            "arm64-ios-simulator", "x86-uwp", "arm-ios", "x64-windows-static-md",
            "arm64-mingw-static", "mips64-linux", "riscv64-linux-release", "arm64-windows-static-md",
            "arm64-linux-release", "x86-freebsd", "x64-windows-release", "s390x-linux",
            "arm64-ios-release", "x86-linux", "x64-openbsd", "arm-windows",
            "x64-mingw-dynamic", "arm-mingw-dynamic", "arm64-osx-release", "loongarch32-linux",
            "riscv64-linux"
        ]
        sbt_triplets = [
            "zig-arm-linux-dynamic",
            "zig-arm-linux-static",
            "zig-arm64-linux-dynamic",
            "zig-arm64-linux-static",
            # musl triplets
            "x64-linux-musl",
            "arm64-linux-musl",
            "arm-linux-musl",
            "riscv64-linux-musl",
            # emscripten with threads
            "wasm32-emscripten-threads",
        ]

        prefix = ""
        suffix = ""
        vcpkg_machine = architectures.get_vcpkg_machine(builder.build_machine)
        vcpkg_platform = builder.build_platform
        vcpkg_liblink = "static" if builder.build_static_libs else "dynamic"
        vcpkg_mode = builder.build_mode

        if builder.build_platform == "web":
            vcpkg_machine = "wasm32"
            vcpkg_platform = "emscripten-threads"

        if builder.build_platform == "windows":
            vcpkg_platform = "mingw"
        
        # For zig with musl, use the generic musl triplets (no zig prefix)
        if builder.libc == "musl" and builder.build_platform == "linux":
            suffix = "-musl"
        elif builder.build_compiler == "zig":
            # For non-musl zig builds, use zig- prefix
            prefix = "zig-"

        triplet_tries = [
            f"{prefix}{vcpkg_machine}-{vcpkg_platform}{suffix}-{vcpkg_liblink}-{vcpkg_mode}",
            f"{prefix}{vcpkg_machine}-{vcpkg_platform}{suffix}-{vcpkg_liblink}",
            f"{prefix}{vcpkg_machine}-{vcpkg_platform}{suffix}-{vcpkg_mode}",
            f"{prefix}{vcpkg_machine}-{vcpkg_platform}{suffix}"
        ]

        for triplet_try in triplet_tries:
            if triplet_try in builtin_triplets \
                  or triplet_try in community_triplets \
                    or triplet_try in sbt_triplets:
                vcpkg_triplet = triplet_try
                break

        logger.info(f"VCPKG triplet auto-detected as: {vcpkg_triplet}")

    if vcpkg_triplet is None:
        raise RuntimeError("no VCPKG triplet detected")

    return vcpkg_triplet

vcpkg_triplet = build_vcpkg_triplet()

def build_vcpkg_manifest():
    # default vcpkg baseline
    vcpkg_baseline = "3a3285c4878c7f5a957202201ba41e6fdeba8db4"
    if hasattr(app, "vcpkg_baseline"):
        vcpkg_baseline = app.vcpkg_baseline
    vcpkg_manifest = {
        "builtin-baseline": f"{vcpkg_baseline}",
        "dependencies": [
        ],
        "overrides": [
        ],
    }

    features = getattr(app, "extlibs_features", {})
    for name, feature_list in getattr(app, f"extlibs_features_{builder.build_platform}", {}).items():
        if name in features:
            features[name].extend(feature_list)
        else:
            features[name] = feature_list

    def add_dependency(name, version):
        if name in features:
            vcpkg_manifest["dependencies"].append({
                "name": name,
                "features": features.get(name)
            })
        else:
            vcpkg_manifest["dependencies"].append(name)
        vcpkg_manifest["overrides"].append({
            "name": name,
            "version": version,
        })
    for libname, libversion in extlibs.items():
        add_dependency(libname, libversion)
    return vcpkg_manifest

def write_vcpkg_manifest(vcpkg_manifest):
    os.makedirs(vcpkg_build_path, exist_ok=True)
    with open(vcpkg_build_manifest_path, "w") as fd:
        json_dump(vcpkg_manifest, fd, indent=2)
    logger.info(f"wrote vcpkg manifest at: {vcpkg_build_manifest_path}")

def __check_vcpkg():
    if os.path.exists(vcpkg_bin_path) is False:
        logger.error(f"VCPKG path does not exist: {vcpkg_bin_path}")
        logger.error(f"deploy VCPKG with: make vcpkg deploy")
        raise RuntimeError("no VCPKG installed")

def execute_vcpkg_install():
    __check_vcpkg()

    from time import time
    start_time = time()

    logger.info("fetching external libraries for {}".format(app.name))

    copy_env = os.environ.copy()
    args = [
        vcpkg_bin_path,
        "install",
        f"--triplet={vcpkg_triplet}",
        "--allow-unsupported",
        f"--overlay-triplets={sbt_triplet_path}",
    ]
    
    # force recompilation
    # if builder.build_platform == "web":
    #     args.append("--no-binarycaching")

    copy_env["VCPKG_BINARY_SOURCES"] = "clear;default"

    if copy_env.get("VCPKG_DEFAULT_BINARY_CACHE", None) is None:
        vcpkg_archive_path = os.path.join(vcpkg_dir_path, "archives")
        copy_env["VCPKG_DEFAULT_BINARY_CACHE"] = vcpkg_archive_path
        if not os.path.isdir(vcpkg_archive_path):
            os.makedirs(vcpkg_archive_path)

    if builder.is_msys():
        args += ["--host-triplet=x64-mingw-dynamic"]

    # if "arm" in vcpkg_triplet:
    #     copy_env["VCPKG_FORCE_SYSTEM_BINARIES"] = "1"
    #     def check_bins(list):
    #         from shutil import which
    #         ret = True
    #         for name in list:
    #             if not which(name):
    #                 logger.error(f"Binary {name} is not present but needed")
    #                 ret = False
    #         return ret
    #     assert(check_bins(["cmake", "ninja", "pkg-config"]))

    if verbose:
        logger.debug(f"executing '{args}' in '{vcpkg_build_path}'")

    from subprocess import run as subprocess_run
    number_of_seconds_per_lib = 180
    proc = subprocess_run(args, cwd=vcpkg_build_path, timeout=(number_of_seconds_per_lib * len(extlibs)), env=copy_env)

    logger.info("fetched in {:.3f} seconds".format(time() - start_time))

    return proc.returncode

def execute_vcpkg_depend_info():
    __check_vcpkg()
    args = [vcpkg_bin_path, "depend-info"] + list(extlibs.keys())
    args += [
        f"--triplet={vcpkg_triplet}",
        f"--overlay-triplets={sbt_triplet_path}",
        "--format=tree",
        "--max-recurse=-1"
    ]
    if verbose:
        logger.debug(f"executing '{args}' in '{vcpkg_build_path}'")

    from subprocess import run as subprocess_run
    proc = subprocess_run(args, cwd=vcpkg_build_path)

    return proc.returncode

def execute_vcpkg_list():
    __check_vcpkg()
    args = (
        vcpkg_bin_path,
        "list",
    )
    if verbose:
        logger.debug(f"executing '{args}' in '{vcpkg_build_path}'")

    from subprocess import run as subprocess_run
    proc = subprocess_run(args, cwd=vcpkg_build_path)

    return proc.returncode

def link_to_extlibs():
    downloaded_path = os.path.join(vcpkg_build_path, "vcpkg_installed", vcpkg_triplet)
    if os.path.exists(downloaded_path):
        # remove existing link:
        if os.path.islink(builder.build_extlib_path):
            os.unlink(builder.build_extlib_path)
        if os.path.exists(builder.build_extlib_path):
            os.unlink(builder.build_extlib_path)
        builder.safe_symlink(downloaded_path, builder.build_extlib_path)

if __name__ == "__main__":
    write_vcpkg_manifest(build_vcpkg_manifest())
    if "fetch" in sys.argv:
        return_code = execute_vcpkg_install()
        if return_code == 0:
            link_to_extlibs()
        sys.exit(return_code)
    elif "list" in sys.argv:
        execute_vcpkg_list()
    elif "tree" in sys.argv:
        execute_vcpkg_depend_info()
