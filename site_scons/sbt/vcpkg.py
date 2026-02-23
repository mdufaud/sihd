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
sbt_overlay_ports_path = f"{builder.sbt_path}/vcpkg/overlay-ports"

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

def _generate_meson_cross_file(overlay_dir: str, extra_c_args: list = None):
    """
    Generate a meson cross file for the current build machine.
    This tells meson it is cross-compiling so it does not attempt to execute
    the compiled binaries on the host (avoids 'Exec format error').
    extra_c_args: additional compiler args to inject into [built-in options] for the host compiler.
    Returns the path to the generated file, or None on failure.
    """
    meson_info = architectures.get_meson_info(builder.build_machine)
    if not meson_info:
        logger.warning(f"no meson info for machine '{builder.build_machine}', skipping meson cross file")
        return None

    cpu_family = meson_info["cpu_family"]
    cpu        = meson_info["cpu"]
    endian     = meson_info["endian"]

    gcc_prefix = architectures.get_gcc_prefix(builder.build_machine, builder.libc)

    cross_file_lines = [
        f"# Auto-generated meson cross file for {builder.build_machine}-{builder.build_platform}-{builder.libc}",
        "[host_machine]",
        f"system = 'linux'",
        f"cpu_family = '{cpu_family}'",
        f"cpu = '{cpu}'",
        f"endian = '{endian}'",
        "",
    ]

    if gcc_prefix:
        cross_file_lines += [
            "[binaries]",
            f"c = '{gcc_prefix}gcc'",
            f"cpp = '{gcc_prefix}g++'",
            f"ar = '{gcc_prefix}ar'",
            f"strip = '{gcc_prefix}strip'",
            f"objcopy = '{gcc_prefix}objcopy'",
            f"pkg-config = 'pkg-config'",
            "",
        ]

    # Inject extra compiler args so that meson's cc.has_header() / cc.find_library()
    # checks can find headers and libs installed by earlier vcpkg deps.
    if extra_c_args:
        args_joined = ", ".join(f"'{a}'" for a in extra_c_args)
        cross_file_lines += [
            "[built-in options]",
            f"c_args = [{args_joined}]",
            f"cpp_args = [{args_joined}]",
            "",
        ]

    cross_file_path = os.path.join(overlay_dir, f"meson-cross-{builder.build_machine}.ini")
    with open(cross_file_path, "w") as f:
        f.write("\n".join(cross_file_lines))

    logger.info(f"generated meson cross file at: {cross_file_path}")
    return cross_file_path


def _generate_cmake_cross_toolchain(overlay_dir: str, vcpkg_installed_inc: str):
    """
    Generate a cmake toolchain file for cross-compilation that:
      - includes the original vcpkg Linux toolchain (compiler setup)
      - sets CMAKE_FIND_ROOT_PATH to include the cross-compiler sysroot
      - sets CMAKE_FIND_ROOT_PATH_MODE_* to prevent cmake from finding host headers/libs
    Returns the path to the generated file, or None on failure.
    """
    vcpkg_root = os.path.dirname(vcpkg_bin_path)
    linux_toolchain = os.path.join(vcpkg_root, "scripts", "toolchains", "linux.cmake")
    linux_toolchain = linux_toolchain.replace("\\", "/")

    if not os.path.isfile(linux_toolchain):
        logger.warning(f"cannot find vcpkg linux toolchain at '{linux_toolchain}', skipping cmake cross toolchain")
        return None

    # Get the cross-compiler sysroot (e.g. /usr/aarch64-linux-gnu)
    gcc_prefix = architectures.get_gcc_prefix(builder.build_machine, builder.libc)
    gcc_cmd = f"{gcc_prefix}gcc" if gcc_prefix else None
    sysroot = None
    if gcc_cmd:
        import subprocess
        try:
            result = subprocess.run(
                [gcc_cmd, "--print-sysroot"],
                capture_output=True, text=True, timeout=5
            )
            if result.returncode == 0 and result.stdout.strip():
                sysroot = result.stdout.strip().replace("\\", "/")
        except (FileNotFoundError, subprocess.TimeoutExpired):
            pass

    vcpkg_installed_inc = vcpkg_installed_inc.replace("\\", "/")

    toolchain_lines = [
        "# Auto-generated cmake cross-compilation toolchain",
        "# Includes the vcpkg Linux toolchain and adds host-header isolation",
        f'include("{linux_toolchain}")',
        "",
        "# Add the cross-compiler sysroot to CMAKE_FIND_ROOT_PATH",
    ]

    if sysroot:
        toolchain_lines.append(f'list(APPEND CMAKE_FIND_ROOT_PATH "{sysroot}")')
        # Set CMAKE_SYSROOT so cmake's PkgConfig module remaps host paths
        # (e.g. /usr/include from host pkg-config results) into the cross sysroot.
        # Without this, pkg_check_modules can inject -isystem /usr/include which
        # causes type conflicts between host and cross headers (e.g. XGenericEventCookie).
        toolchain_lines.append(f'set(CMAKE_SYSROOT "{sysroot}")')

    toolchain_lines += [
        "",
        "# Prevent cmake find_* commands from searching host system paths.",
        "# vcpkg.cmake will add vcpkg_installed to CMAKE_FIND_ROOT_PATH automatically.",
        'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)',
        'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)',
        'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)',
        'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)',
        "",
    ]

    toolchain_path = os.path.join(overlay_dir, f"cmake-cross-{builder.build_machine}.cmake")
    with open(toolchain_path, "w") as f:
        f.write("\n".join(toolchain_lines))

    logger.info(f"generated cmake cross toolchain at: {toolchain_path}")
    return toolchain_path


def build_overlay_triplet_with_flags():
    """
    Generate a wrapper triplet file that includes the original triplet and:
      - appends extra VCPKG_C_FLAGS / VCPKG_CXX_FLAGS from app.py
      - injects a meson cross file when cross-compiling on Linux
      - injects a cmake cross-toolchain to prevent host-header contamination
      - injects per-port CMake configure options (base + platform override)
    Returns the overlay directory path, or None if no overlay is needed.
    """
    cflags   = getattr(app, "vcpkg_cflags", [])
    cxxflags = getattr(app, "vcpkg_cxxflags", [])
    need_flags = bool(cflags or cxxflags)

    # Generate cross-compilation helpers when cross-compiling for Linux
    is_cross_linux = builder.is_cross_building() and builder.build_platform == "linux"

    # Per-port CMake configure options: base config (restrictive, safe for cross)
    cmake_opts = dict(getattr(app, "vcpkg_cmake_configure_options", {}))
    # Platform-specific overrides:
    # - Native builds: re-enable system-available features (e.g. SDL3 X11 on native Linux)
    # - Cross builds: apply cross-platform-specific overrides (e.g. re-enable X11/Wayland from vcpkg)
    if builder.is_cross_building():
        for port_name, opts in getattr(app, f"vcpkg_cmake_configure_options_cross_{builder.build_platform}", {}).items():
            cmake_opts[port_name] = opts
    else:
        for port_name, opts in getattr(app, f"vcpkg_cmake_configure_options_{builder.build_platform}", {}).items():
            cmake_opts[port_name] = opts
    need_cmake_opts = any(opts for opts in cmake_opts.values())

    if not need_flags and not is_cross_linux and not need_cmake_opts:
        return None

    overlay_dir = os.path.join(vcpkg_build_path, "overlay-triplets")
    os.makedirs(overlay_dir, exist_ok=True)

    # Resolve the original triplet file path.
    # SBT custom triplets take priority (may contain platform-specific fixes,
    # e.g. emscripten pthread flags via VCPKG_CMAKE_CONFIGURE_OPTIONS),
    # then fall back to vcpkg builtin/community triplets.
    vcpkg_root = os.path.dirname(vcpkg_bin_path)
    orig_candidates = [
        os.path.join(sbt_triplet_path, f"{vcpkg_triplet}.cmake"),
        os.path.join(vcpkg_root, "triplets", f"{vcpkg_triplet}.cmake"),
        os.path.join(vcpkg_root, "triplets", "community", f"{vcpkg_triplet}.cmake"),
    ]
    orig_triplet = None
    for candidate in orig_candidates:
        if os.path.isfile(candidate):
            orig_triplet = candidate
            break

    if orig_triplet is None:
        logger.warning(f"cannot find original triplet file for '{vcpkg_triplet}', skipping overlay generation")
        return None

    lines = [
        "# Auto-generated overlay triplet",
        f'include("{orig_triplet}")',
        "",
    ]

    # vcpkg requires VCPKG_C_FLAGS and VCPKG_CXX_FLAGS to be set together
    if need_flags:
        c_flags_str   = " ".join(cflags)   if cflags   else ""
        cxx_flags_str = " ".join(cxxflags) if cxxflags else ""
        lines.append(f'string(APPEND VCPKG_C_FLAGS " {c_flags_str}")')
        lines.append(f'string(APPEND VCPKG_CXX_FLAGS " {cxx_flags_str}")')
        lines.append("")

    # Cross-compilation for Linux: meson cross file + cmake cross toolchain
    if is_cross_linux:
        # Force vcpkg to build X11/Wayland libraries from source
        # (instead of producing empty packages that expect system libs)
        lines.append("# Force vcpkg X11/Wayland libraries for cross-compilation")
        lines.append("set(X_VCPKG_FORCE_VCPKG_X_LIBRARIES ON)")
        lines.append("set(X_VCPKG_FORCE_VCPKG_WAYLAND_LIBRARIES ON)")
        lines.append("")

        # Build the include path for vcpkg-installed cross deps so that meson's
        # cc.has_header() checks (e.g. sys/capability.h from libcap) can find them.
        vcpkg_installed_inc = os.path.join(vcpkg_build_path, "vcpkg_installed", vcpkg_triplet, "include")
        vcpkg_installed_inc = vcpkg_installed_inc.replace("\\", "/")
        meson_extra_args = [f"-I{vcpkg_installed_inc}"]

        # Meson cross file so meson detects cross-compilation correctly
        meson_cross_path = _generate_meson_cross_file(overlay_dir, extra_c_args=meson_extra_args)
        if meson_cross_path:
            cmake_path = meson_cross_path.replace("\\", "/")
            lines.append(f'set(VCPKG_MESON_CROSS_FILE_RELEASE "{cmake_path}")')
            lines.append(f'set(VCPKG_MESON_CROSS_FILE_DEBUG   "{cmake_path}")')
            lines.append("")

        # Also add the include path via VCPKG_C/CXX_FLAGS so cmake-based packages find it too
        lines.append(f'string(APPEND VCPKG_C_FLAGS " -I{vcpkg_installed_inc}")')
        lines.append(f'string(APPEND VCPKG_CXX_FLAGS " -I{vcpkg_installed_inc}")')
        lines.append("")

        # CMake cross-toolchain to prevent host-header contamination.
        # Sets CMAKE_FIND_ROOT_PATH_MODE_* = ONLY so cmake find_* commands
        # only search within the cross sysroot and vcpkg installed dirs.
        cmake_toolchain_path = _generate_cmake_cross_toolchain(overlay_dir, vcpkg_installed_inc)
        if cmake_toolchain_path:
            cmake_toolchain_path = cmake_toolchain_path.replace("\\", "/")
            lines.append(f'set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "{cmake_toolchain_path}")')
            lines.append("")

    # Apply per-port CMake configure options (base + platform-resolved)
    # Ports with empty option lists are skipped (platform override cleared them)
    active_cmake_opts = {k: v for k, v in cmake_opts.items() if v}

    if active_cmake_opts:
        for port_name, opts in active_cmake_opts.items():
            lines.append(f'if(PORT STREQUAL "{port_name}")')
            for opt in opts:
                lines.append(f'  list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS "{opt}")')
            lines.append(f'endif()')
        lines.append("")

    wrapper_path = os.path.join(overlay_dir, f"{vcpkg_triplet}.cmake")
    with open(wrapper_path, "w") as f:
        f.write("\n".join(lines))

    logger.info(f"generated overlay triplet at: {wrapper_path}")

    # For cross-linux: also generate a host triplet overlay with X11/Wayland force flags.
    # Some X11 packages (e.g. libx11) build host tools (makekeys) that are needed
    # by the cross-build. Without the force flag on the host triplet, the host
    # build produces an empty package and the cross-build fails.
    if is_cross_linux:
        host_triplet = "x64-linux"
        vcpkg_root = os.path.dirname(vcpkg_bin_path)
        host_orig_candidates = [
            os.path.join(vcpkg_root, "triplets", f"{host_triplet}.cmake"),
            os.path.join(vcpkg_root, "triplets", "community", f"{host_triplet}.cmake"),
        ]
        host_orig = None
        for candidate in host_orig_candidates:
            if os.path.isfile(candidate):
                host_orig = candidate
                break
        if host_orig:
            host_lines = [
                "# Auto-generated host overlay triplet for cross-linux X11/Wayland",
                f'include("{host_orig}")',
                "",
                "# Force-build X11/Wayland libs so that host tools (e.g. makekeys from libx11)",
                "# are available for cross-compilation targets.",
                "set(X_VCPKG_FORCE_VCPKG_X_LIBRARIES ON)",
                "set(X_VCPKG_FORCE_VCPKG_WAYLAND_LIBRARIES ON)",
                "",
            ]
            host_wrapper_path = os.path.join(overlay_dir, f"{host_triplet}.cmake")
            with open(host_wrapper_path, "w") as f:
                f.write("\n".join(host_lines))
            logger.info(f"generated host overlay triplet at: {host_wrapper_path}")

    return overlay_dir


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
    # Platform-specific features (e.g. libusb[udev] on linux).
    # Only applied on native (non-cross) builds; cross targets use base features only
    # since target sysroot may not have the required system libraries.
    if not builder.is_cross_building():
        for name, feature_list in getattr(app, f"extlibs_features_{builder.build_platform}", {}).items():
            if name in features:
                features[name].extend(feature_list)
            else:
                features[name] = feature_list

    # Libraries for which vcpkg default-features should be disabled (e.g. when cross-compiling)
    # This also adds the library to the manifest if it's not already there (transitive dep override)
    no_default_features = set()
    if builder.is_cross_building():
        no_default_features = set(getattr(app, "vcpkg_no_default_features", []))

    def add_dependency(name, version):
        dep = {"name": name}
        if name in features:
            dep["features"] = features.get(name)
        if name in no_default_features:
            dep["default-features"] = False
            no_default_features.discard(name)
        # Use dict form if we have more than just the name
        if len(dep) > 1:
            vcpkg_manifest["dependencies"].append(dep)
        else:
            vcpkg_manifest["dependencies"].append(name)
        if version:
            vcpkg_manifest["overrides"].append({
                "name": name,
                "version": version,
            })
    for libname, libversion in extlibs.items():
        add_dependency(libname, libversion)

    # Cross-linux: add X11/Wayland vcpkg packages (built from source for the target)
    if builder.is_cross_building() and builder.build_platform == "linux":
        cross_extlibs = getattr(app, "vcpkg_cross_linux_extlibs", {})
        cross_features = getattr(app, "vcpkg_cross_linux_extlibs_features", {})
        for libname, libversion in cross_extlibs.items():
            dep = {"name": libname}
            if libname in cross_features:
                dep["features"] = cross_features[libname]
            if len(dep) > 1:
                vcpkg_manifest["dependencies"].append(dep)
            else:
                vcpkg_manifest["dependencies"].append(libname)
            if libversion:
                vcpkg_manifest["overrides"].append({
                    "name": libname,
                    "version": libversion,
                })

    # Add remaining no_default_features libs that weren't in extlibs (transitive dep overrides)
    no_default_features_add = {}
    if builder.is_cross_building():
        no_default_features_add = getattr(app, "vcpkg_no_default_features_add", {})
    for name in no_default_features:
        dep = {"name": name, "default-features": False}
        dep_features = no_default_features_add.get(name, [])
        if name in features:
            dep_features = features[name] + dep_features
        if dep_features:
            dep["features"] = dep_features
        vcpkg_manifest["dependencies"].append(dep)

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

    # Cross-linux: create a wrapper pkg-config script that isolates
    # pkg-config from host system .pc files.
    #
    # Root cause: vcpkg.cmake adds "/" to CMAKE_PREFIX_PATH when
    # CMAKE_FIND_ROOT_PATH_MODE_* is ONLY. cmake's FindPkgConfig module then
    # appends /lib/pkgconfig (which exists on the host) to PKG_CONFIG_PATH.
    # Since PKG_CONFIG_PATH is searched BEFORE PKG_CONFIG_LIBDIR, the system
    # egl.pc (and others) leak -I/usr/include into cross-builds, causing
    # conflicting type errors.
    #
    # The wrapper filters PKG_CONFIG_PATH to keep only vcpkg-related paths
    # (vcpkg packages dir, vcpkg installed dir) and sets PKG_CONFIG_LIBDIR
    # to vcpkg paths. This preserves vcpkg's own z_vcpkg_setup_pkgconfig_path
    # settings while blocking cmake-added system paths like /lib/pkgconfig.
    if builder.is_cross_building() and builder.build_platform == "linux":
        vcpkg_installed_base = os.path.join(vcpkg_build_path, "vcpkg_installed", vcpkg_triplet)
        overlay_dir = os.path.join(vcpkg_build_path, "overlay-triplets")
        os.makedirs(overlay_dir, exist_ok=True)

        wrapper_path = os.path.join(overlay_dir, "pkg-config-cross-wrapper")
        real_pkgconfig = "/bin/pkg-config"

        pkgconfig_dirs = ":".join([
            f"{vcpkg_installed_base}/lib/pkgconfig",
            f"{vcpkg_installed_base}/debug/lib/pkgconfig",
            f"{vcpkg_installed_base}/share/pkgconfig",
        ])

        # The vcpkg root and build paths — any PKG_CONFIG_PATH entry containing
        # these prefixes is a vcpkg path and should be kept.
        vcpkg_root = os.path.dirname(vcpkg_bin_path)

        with open(wrapper_path, "w") as f:
            f.write('#!/bin/sh\n')
            f.write('# Auto-generated pkg-config wrapper for cross-linux builds.\n')
            f.write('# Filters out host system paths from PKG_CONFIG_PATH.\n')
            f.write(f'export PKG_CONFIG_LIBDIR="{pkgconfig_dirs}"\n')
            f.write('\n')
            f.write('# Filter PKG_CONFIG_PATH: keep only vcpkg-related entries.\n')
            f.write('# vcpkg port scripts (z_vcpkg_setup_pkgconfig_path) and\n')
            f.write('# vcpkg_fixup_pkgconfig set PKG_CONFIG_PATH to vcpkg package/install dirs.\n')
            f.write('# cmake\'s FindPkgConfig also appends CMAKE_PREFIX_PATH-derived system paths.\n')
            f.write('# We keep vcpkg paths and drop everything else.\n')
            f.write('FILTERED=""\n')
            f.write('OLDIFS="$IFS"\n')
            f.write('IFS=":"\n')
            f.write('for dir in $PKG_CONFIG_PATH; do\n')
            f.write(f'  case "$dir" in\n')
            f.write(f'    {vcpkg_root}/*|{vcpkg_build_path}/*) FILTERED="${{FILTERED:+$FILTERED:}}$dir" ;;\n')
            f.write(f'  esac\n')
            f.write('done\n')
            f.write('IFS="$OLDIFS"\n')
            f.write('export PKG_CONFIG_PATH="$FILTERED"\n')
            f.write(f'exec {real_pkgconfig} "$@"\n')
        os.chmod(wrapper_path, 0o755)

        copy_env["PKG_CONFIG"] = wrapper_path
        logger.info(f"using pkg-config wrapper: {wrapper_path}")

    args = [
        vcpkg_bin_path,
        "install",
        f"--triplet={vcpkg_triplet}",
        "--allow-unsupported",
    ]

    # Inject extra compiler flags via an overlay triplet (if defined in app.py)
    overlay_flags_dir = build_overlay_triplet_with_flags()
    if overlay_flags_dir:
        args.append(f"--overlay-triplets={overlay_flags_dir}")

    args.append(f"--overlay-triplets={sbt_triplet_path}")

    # Custom overlay ports (e.g. libxcursor which has no official vcpkg port)
    if os.path.isdir(sbt_overlay_ports_path):
        args.append(f"--overlay-ports={sbt_overlay_ports_path}")

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
    if os.path.isdir(sbt_overlay_ports_path):
        args.append(f"--overlay-ports={sbt_overlay_ports_path}")
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

def build_cross_linux_foundation_manifest():
    """
    Build a minimal manifest containing only X11/Wayland foundation packages.
    These must be installed BEFORE the main manifest because ports like glfw3/sdl3
    use cmake's FindX11/FindWayland to locate them at configure time, but don't
    declare vcpkg dependencies on them (they expect system packages).
    """
    vcpkg_baseline_val = "3a3285c4878c7f5a957202201ba41e6fdeba8db4"
    if hasattr(app, "vcpkg_baseline"):
        vcpkg_baseline_val = app.vcpkg_baseline

    cross_extlibs = getattr(app, "vcpkg_cross_linux_extlibs", {})
    cross_features = getattr(app, "vcpkg_cross_linux_extlibs_features", {})

    manifest = {
        "builtin-baseline": vcpkg_baseline_val,
        "dependencies": [],
        "overrides": [],
    }

    for libname, libversion in cross_extlibs.items():
        dep = {"name": libname}
        if libname in cross_features:
            dep["features"] = cross_features[libname]
        if len(dep) > 1:
            manifest["dependencies"].append(dep)
        else:
            manifest["dependencies"].append(libname)
        if libversion:
            manifest["overrides"].append({"name": libname, "version": libversion})

    return manifest


if __name__ == "__main__":
    write_vcpkg_manifest(build_vcpkg_manifest())
    if "fetch" in sys.argv:
        # Cross-linux: install X11/Wayland foundation packages first.
        # Ports like glfw3/sdl3 use cmake FindX11/FindWayland at configure time
        # but don't declare vcpkg dependencies on them. Without a two-phase install,
        # vcpkg may schedule glfw3 before libx11, causing FindX11 to fail.
        if builder.is_cross_building() and builder.build_platform == "linux":
            cross_extlibs = getattr(app, "vcpkg_cross_linux_extlibs", {})
            if cross_extlibs:
                logger.info("cross-linux phase 1: installing X11/Wayland foundation packages")
                foundation_manifest = build_cross_linux_foundation_manifest()
                write_vcpkg_manifest(foundation_manifest)
                return_code = execute_vcpkg_install()
                if return_code != 0:
                    logger.error("cross-linux phase 1 failed")
                    sys.exit(return_code)
                logger.info("cross-linux phase 2: installing all packages")
                write_vcpkg_manifest(build_vcpkg_manifest())

        return_code = execute_vcpkg_install()
        if return_code == 0:
            link_to_extlibs()
        sys.exit(return_code)
    elif "list" in sys.argv:
        execute_vcpkg_list()
    elif "tree" in sys.argv:
        execute_vcpkg_depend_info()
