"""
Application-specific vcpkg port configuration.

This file contains vcpkg build options that are specific to the ports used
by this project. These are NOT generic build system settings — they control
how individual vcpkg ports are configured (cmake options, compiler workarounds,
cross-linux X11/Wayland packages, etc.).

Imported by app.py to keep port-level vcpkg details separate from the main
module/dependency declarations.
"""

###############################################################################
# vcpkg compiler flags
###############################################################################

# Extra flags injected into VCPKG_C_FLAGS / VCPKG_CXX_FLAGS via overlay triplet.
vcpkg_cflags = [
    "-Wno-error=discarded-qualifiers",   # libwebsockets v4.5.2 + GCC >= 14
]
vcpkg_cxxflags = []

###############################################################################
# vcpkg per-port cmake configure options
###############################################################################

# Default config for cross-compilation — safe baseline that disables
# system-dependent features. Platform-specific variants below can re-enable them.
vcpkg_cmake_configure_options = {
    "dbus": [
        "-DDBUS_SESSION_SOCKET_DIR=/tmp",
    ],
    # SDL3: disable system-dependent features by default.
    "sdl3": [
        "-DSDL_X11=OFF", "-DSDL_WAYLAND=OFF", "-DSDL_IBUS=OFF",
        "-DSDL_JACK=OFF", "-DSDL_PULSEAUDIO=OFF", "-DSDL_PIPEWIRE=OFF",
        "-DSDL_SNDIO=OFF", "-DSDL_KMSDRM=OFF",
        "-DSDL_OPENGL=OFF", "-DSDL_OPENGLES=OFF",
        "-DSDL_LIBUDEV=OFF", "-DSDL_LIBURING=OFF",
    ],
    # glfw3 enables X11 by default on Linux (REQUIRED) which fails without X11 headers
    "glfw3": ["-DGLFW_BUILD_X11=OFF"],
    # imgui: prevent glfw3.h from including GL/gl.h (imgui has its own GL3 loader)
    "imgui": ["-DCMAKE_CXX_FLAGS_INIT=-DGLFW_INCLUDE_NONE"],
}

# Re-enable system features on native Linux (X11/Wayland/etc. from system packages)
vcpkg_cmake_configure_options_linux = {
    "sdl3": [],
    "glfw3": [],
    "imgui": [],
}

# Cross-linux: re-enable X11/Wayland (headers now from vcpkg).
# Audio backends (JACK, PulseAudio...), ibus, udev etc. remain disabled (no vcpkg ports).
vcpkg_cmake_configure_options_cross_linux = {
    "sdl3": [
        # X11 and Wayland: enabled (built from vcpkg)
        # OpenGL: enabled (headers from opengl vcpkg package)
        "-DSDL_IBUS=OFF",
        "-DSDL_JACK=OFF", "-DSDL_PULSEAUDIO=OFF", "-DSDL_PIPEWIRE=OFF",
        "-DSDL_SNDIO=OFF", "-DSDL_KMSDRM=OFF",
        "-DSDL_OPENGLES=OFF",
        "-DSDL_LIBUDEV=OFF", "-DSDL_LIBURING=OFF",
        "-DSDL_WAYLAND_LIBDECOR=OFF",  # no vcpkg port for libdecor
        # Pre-set HAVE_XGENERICEVENT: the check_c_source_compiles test fails because
        # static libX11.a needs transitive deps (libxcb, libXau...) for linking, but
        # SDL3's cmake only passes libX11.a. The headers ARE correct.
        "-DHAVE_XGENERICEVENT=1",
    ],
    "glfw3": [],  # X11 from vcpkg, use GLFW defaults (X11=ON)
    "imgui": ["-DCMAKE_CXX_FLAGS_INIT=-DGLFW_INCLUDE_NONE"],
}

###############################################################################
# vcpkg default-features control
###############################################################################

# Libraries whose default-features should be disabled in vcpkg manifest (cross-compilation only).
# Only explicitly listed features from extlibs_features will be used.
vcpkg_no_default_features = [
    "dbus",    # systemd default pulls libsystemd which doesn't build on musl (glibc printf.h)
    "libusb",  # udev default requires libudev headers (no vcpkg port for libudev exists)
]

###############################################################################
# Cross-linux display packages
###############################################################################

# Additional vcpkg packages needed only for cross-linux builds.
# On native Linux, X11/Wayland come from system packages.
# On cross-Linux (aarch64, riscv64, arm32...), vcpkg builds them from source
# using X_VCPKG_FORCE_VCPKG_X_LIBRARIES and X_VCPKG_FORCE_VCPKG_WAYLAND_LIBRARIES.
vcpkg_cross_linux_extlibs = {
    # X11 core + extensions
    "libx11": "",       # core X11 (transitive: xcb, xproto, xtrans, libxau, libxdmcp, xorg-macros)
    "libxext": "",      # X11 extension library (Xshape, Xshm...)
    "libxi": "",        # X Input extension (gamepad, touch...)
    "libxinerama": "",  # multi-monitor support
    "libxcursor": "",   # cursor management (needed by GLFW3, dlopen'd at runtime)
    "libxrandr": "",    # resolution/rotation (transitive: libxrender, libxfixes)
    "libxkbcommon": "", # keyboard handling (needed by both X11 and Wayland)
    # Wayland
    "wayland": "",            # core Wayland (requires force-build for cross)
    "wayland-protocols": "",  # Wayland protocol extensions (requires force-build for cross)
}

vcpkg_cross_linux_extlibs_features = {
    "wayland": ["force-build"],
    "wayland-protocols": ["force-build"],
}
