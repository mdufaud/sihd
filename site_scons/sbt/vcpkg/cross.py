"""
Cross-compilation helpers for vcpkg.

Handles:
  - Meson cross file generation
  - CMake cross toolchain generation
  - Overlay triplet generation (flags, cross-linux isolation, per-port options)
  - pkg-config wrapper for cross-linux builds
"""

import os
import shutil

from sbt import architectures
from sbt import builder
from sbt import logger
from sbt.vcpkg import triplets


def generate_cmake_cross_toolchain(overlay_dir: str, vcpkg_installed_inc: str, vcpkg_bin_path: str) -> str:
    """
    Generate a cmake toolchain file for cross-compilation that:
      - includes the original vcpkg Linux toolchain (compiler setup)
      - explicitly sets cross-compiler paths (important for same-architecture
        cross-builds like musl where vcpkg's linux.cmake doesn't auto-detect)
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
        "",
    ]

    # Explicitly set cross-compiler tools BEFORE including linux.cmake.
    # This is critical for same-architecture cross-builds (e.g. musl on x86_64):
    # vcpkg's linux.cmake only auto-detects the cross-compiler when
    # CMAKE_HOST_SYSTEM_PROCESSOR != CMAKE_SYSTEM_PROCESSOR, so musl builds
    # would otherwise fall back to /bin/cc.
    if gcc_prefix:
        toolchain_lines.append("# Cross-compiler tools (set before linux.cmake so they take precedence)")
        toolchain_lines.append(f'set(CMAKE_C_COMPILER "{gcc_prefix}gcc")')
        toolchain_lines.append(f'set(CMAKE_CXX_COMPILER "{gcc_prefix}g++")')
        # Only set optional tools if they actually exist on the system.
        # Musl toolchains (e.g. Arch's musl package) only provide gcc/g++/ar/ranlib
        # and do NOT ship objcopy, strip, nm, ld — the host binutils work fine.
        _optional_tools = {
            "CMAKE_AR":      f"{gcc_prefix}ar",
            "CMAKE_RANLIB":  f"{gcc_prefix}ranlib",
            "CMAKE_STRIP":   f"{gcc_prefix}strip",
            "CMAKE_OBJCOPY": f"{gcc_prefix}objcopy",
            "CMAKE_NM":      f"{gcc_prefix}nm",
            "CMAKE_LINKER":  f"{gcc_prefix}ld",
        }
        for cmake_var, tool_cmd in _optional_tools.items():
            if shutil.which(tool_cmd):
                toolchain_lines.append(f'set({cmake_var} "{tool_cmd}" CACHE FILEPATH "{cmake_var}")')
        toolchain_lines.append("")

    toolchain_lines += [
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


def build_overlay_triplet_with_flags(app, vcpkg_triplet: str, vcpkg_build_path: str,
                                     vcpkg_bin_path: str, sbt_triplet_path: str,
                                     addon_triplet_path: str = None) -> str:
    """
    Generate a wrapper triplet file that includes the original triplet and:
      - appends extra VCPKG_C_FLAGS / VCPKG_CXX_FLAGS from app.py
      - injects a meson cross file when cross-compiling on Linux
      - injects a cmake cross-toolchain to prevent host-header contamination
      - injects per-port CMake configure options (base + platform override)
    For dynamically-generated triplets (musl, zig), the base content is generated
    inline from architectures.py — no static .cmake file needed.
    Returns the overlay directory path (always created for dynamic triplets).
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

    # Check if this triplet needs dynamic generation (musl/zig)
    is_dynamic = triplets.is_musl_triplet(vcpkg_triplet) or triplets.is_zig_triplet(vcpkg_triplet)

    if not need_flags and not is_cross_linux and not need_cmake_opts and not is_dynamic:
        return None

    overlay_dir = os.path.join(vcpkg_build_path, "overlay-triplets")
    os.makedirs(overlay_dir, exist_ok=True)

    # For dynamic triplets (musl/zig): generate the base triplet first,
    # then apply overlay modifications on top.
    if is_dynamic:
        dynamic_path = triplets.generate_dynamic_triplet(overlay_dir, vcpkg_triplet)
        if dynamic_path is None:
            logger.warning(f"failed to generate dynamic triplet for '{vcpkg_triplet}'")
            return None

        # If no additional modifications needed, the dynamic triplet is ready
        if not need_flags and not is_cross_linux and not need_cmake_opts:
            return overlay_dir

        # Read the generated content and append modifications
        with open(dynamic_path, "r") as f:
            lines = f.read().rstrip("\n").split("\n")
        lines.append("")
    else:
        # Static triplet: resolve the original triplet file and include it
        vcpkg_root = os.path.dirname(vcpkg_bin_path)
        orig_candidates = [
            os.path.join(sbt_triplet_path, f"{vcpkg_triplet}.cmake"),
        ]
        if addon_triplet_path:
            orig_candidates.append(os.path.join(addon_triplet_path, f"{vcpkg_triplet}.cmake"))
        orig_candidates += [
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

    # Cross-compilation for Linux: cmake cross toolchain for host-header isolation
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

        # Add the include path via VCPKG_C/CXX_FLAGS so both cmake and meson packages find it
        lines.append(f'string(APPEND VCPKG_C_FLAGS " -I{vcpkg_installed_inc}")')
        lines.append(f'string(APPEND VCPKG_CXX_FLAGS " -I{vcpkg_installed_inc}")')
        lines.append("")

        # NOTE: We intentionally do NOT set VCPKG_MESON_CROSS_FILE here.
        # vcpkg's meson integration auto-generates native/cross files that include
        # ADDITIONAL_BINARIES (e.g. wayland_scanner) from port dependencies.
        # Overriding with our own cross file would lose these binaries because
        # meson's find_program() only searches cross file [binaries] in cross mode.
        # Instead, the cmake cross-toolchain (below) ensures the correct compiler
        # is detected by vcpkg_cmake_get_vars, which propagates to the meson files.

        # CMake cross-toolchain to prevent host-header contamination.
        # Sets CMAKE_FIND_ROOT_PATH_MODE_* = ONLY so cmake find_* commands
        # only search within the cross sysroot and vcpkg installed dirs.
        # Also sets CMAKE_C_COMPILER/CMAKE_CXX_COMPILER for same-architecture
        # cross-builds (musl) where vcpkg's linux.cmake doesn't auto-detect.
        cmake_toolchain_path = generate_cmake_cross_toolchain(overlay_dir, vcpkg_installed_inc, vcpkg_bin_path)
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
        _generate_host_overlay_triplet(overlay_dir, vcpkg_bin_path)

    return overlay_dir


def _generate_host_overlay_triplet(overlay_dir: str, vcpkg_bin_path: str):
    """Generate a host triplet overlay with X11/Wayland force flags for cross-linux builds."""
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


def generate_pkgconfig_wrapper(overlay_dir: str, vcpkg_triplet: str,
                               vcpkg_build_path: str, vcpkg_bin_path: str) -> str:
    """
    Create a wrapper pkg-config script that isolates pkg-config from host system .pc files.

    Root cause: vcpkg.cmake adds "/" to CMAKE_PREFIX_PATH when
    CMAKE_FIND_ROOT_PATH_MODE_* is ONLY. cmake's FindPkgConfig module then
    appends /lib/pkgconfig (which exists on the host) to PKG_CONFIG_PATH.
    Since PKG_CONFIG_PATH is searched BEFORE PKG_CONFIG_LIBDIR, the system
    egl.pc (and others) leak -I/usr/include into cross-builds.

    The wrapper filters PKG_CONFIG_PATH to keep only vcpkg-related paths.
    Returns the wrapper path.
    """
    vcpkg_installed_base = os.path.join(vcpkg_build_path, "vcpkg_installed", vcpkg_triplet)
    os.makedirs(overlay_dir, exist_ok=True)

    wrapper_path = os.path.join(overlay_dir, "pkg-config-cross-wrapper")
    real_pkgconfig = "/bin/pkg-config"

    pkgconfig_dirs = ":".join([
        f"{vcpkg_installed_base}/lib/pkgconfig",
        f"{vcpkg_installed_base}/debug/lib/pkgconfig",
        f"{vcpkg_installed_base}/share/pkgconfig",
    ])

    vcpkg_root = os.path.dirname(vcpkg_bin_path)

    with open(wrapper_path, "w") as f:
        f.write('#!/bin/sh\n')
        f.write('# Auto-generated pkg-config wrapper for cross-linux builds.\n')
        f.write('# Filters out host system paths from PKG_CONFIG_PATH.\n')
        f.write(f'export PKG_CONFIG_LIBDIR="{pkgconfig_dirs}"\n')
        f.write('\n')
        f.write('# Filter PKG_CONFIG_PATH: keep only vcpkg-related entries.\n')
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

    logger.info(f"using pkg-config wrapper: {wrapper_path}")
    return wrapper_path
