###############################################################################
# external libs
###############################################################################

extlibs = {
    # unit test
    "gtest": "1.17.0#2",
    # json parsing
    "simdjson": "4.6.4",
    # util
    "cli11": "2.6.1",
    "fmt": "12.1.0",
    "libuuid": "1.0.3#15",
    # http
    "libwebsockets": "4.5.2",
    "curl": "7.87.0",
    "openssl": "3.1.0",
    "libssh": "0.10.6",
    "libcap": "2.70",
    "libuv": "1.46.0",
    "zlib": "1.3.1",
    # pcap
    "libpcap": "1.10.5",
    # usb
    "libusb": "1.0.27",
    # gui
    "opengl": "",  # provides GL/gl.h headers for cross-compilation (via opengl-registry)
    "ftxui": "6.1.9",
    "imgui": "1.91.9",
    "ncurses": "6.5#3", # ncursesw
    "libxcrypt": "4.5.2", # fixes a compilation issue with imgui
    # bindings
    "pybind11": "3.0.1",
    # compressing utility
    "libzip": "1.7.3",
    # other
    "libjpeg": "9d",
    "lua": "5.3.5-5",
    "luabridge3": "3.0-rc3",
    # bt
    "simpleble": "0.8.1#1",
}

# glfw needs: libxi-dev libxinerama-dev libxcursor-dev xorg libglu1-mesa pkg-config
extlibs_features = {
    "imgui": ["glfw-binding", "opengl3-binding", "sdl3-binding"],
}

extlibs_features_linux = {
    "libusb": ["udev"],  # udev is linux-only
}

extlibs_features_windows = {
    "imgui": ["glfw-binding", "opengl3-binding", "sdl3-binding", "win32-binding", "dx11-binding"],
}

extlibs_features_android = {
    "imgui": ["android-binding", "opengl3-binding"],
}

extlibs_features_web = {
    "imgui": ["sdl3-binding", "opengl3-binding"],
}

# on windows some libs are not available through vcpkg
extlibs_skip_windows = [
    "dbus",
    "libcap",
    "simpleble",
]

# on web: those libs don't compile properly with emscripten threading
extlibs_skip_web = [
    "openssl",
    "libwebsockets",
    "curl",
    "libssh",
    "libpcap",
    "libusb",
    "libxcrypt",
    "ftxui",
    "opengl",
    "glfw3",
    "dbus",
    "simpleble",
    "libcap",
]

# on android: libs that are linux-only or not relevant
extlibs_skip_android = [
    "dbus",
    "libcap",
    "libwebsockets",
    "curl",
    "libssh",
    "libpcap",
    "libusb",
    "libxcrypt",
    "ftxui",
    "opengl",
    "glfw3",
    "simpleble",
]

vcpkg_baseline = "04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4"

# Declarative overlays over stock vcpkg ports (replaces hand-written overlay-ports/).
# Schema: patches / remove_patches / manifest / files / recipe_patches (see sbt/vcpkg/patches.py).
vcpkg_ports = {
    "dbus": {
        "remove_patches": ["session-socket-dir.diff"],
        "manifest": {"port-version": 2, "default-features": []},
        "recipe_patches": ["patches/dbus/recipe.patch"],
    },
    "libwebsockets": {
        "patches": [
            "patches/libwebsockets/mingw-pthreads.patch",
            "patches/libwebsockets/fix-smp-event-pipes.patch",
            "patches/libwebsockets/fix-gcc15-const.patch",
        ],
        "manifest": {"version-semver": "4.5.2"},
        "recipe_patches": ["patches/libwebsockets/recipe.patch"],
    },
    "libcap": {
        "recipe_patches": ["patches/libcap/recipe.patch"],
    },
    "ncurses": {
        "recipe_patches": ["patches/ncurses/recipe.patch"],
    },
    "python3": {
        "files": {"files/python3/0012-force-disable-modules.patch": "0012-force-disable-modules.patch"},
        "patches": ["patches/python3/no-checksharedmods.patch"],
        "recipe_patches": ["patches/python3/recipe.patch"],
    },
    "lua": {
        "files": {
            "files/lua/CONTROL": "CONTROL",
            "files/lua/COPYRIGHT": "COPYRIGHT",
            "files/lua/vcpkg-cmake-wrapper.cmake.in": "vcpkg-cmake-wrapper.cmake.in",
        },
        "recipe_patches": ["patches/lua/recipe.patch"],
    },
    "libxslt": {
        # pulled target-side by libx11; lld (zig triplets) errors on the
        # version-script symbols LIBXSLT_WITH_DEBUGGER=OFF compiles out
        "recipe_patches": ["patches/libxslt/recipe.patch"],
    },
    "curl": {
        # zig triplets: the HAVE_LIBSOCKET probe false-positives (test exe links static)
        # and the real shared link dies on libsocket, which no linux target ships
        "recipe_patches": ["patches/curl/recipe.patch"],
    },
    "libssh": {
        # zig only: fresh gcc builds under cmake 4.4.3 regress (ProxyCommand probe)
        "compilers": ["zig"],
        "recipe_patches": ["patches/libssh/recipe.patch"],
    },
    "libzip": {
        # zig only: fresh gcc builds under cmake 4.4.3 regress (arm64-dynamic shared link)
        "compilers": ["zig"],
        "recipe_patches": ["patches/libzip/recipe.patch"],
    },
    # glfw3/sdl3 probe X11/Wayland headers with cmake FindX11 at configure time
    # without declaring them: on cross-linux the two-phase foundation install can
    # be torn down mid-plan before they configure. Declaring the deps here pins
    # the ordering in vcpkg's graph (no-op ports on native linux).
    "glfw3": {
        "recipe_patches": ["patches/glfw3/recipe.patch"],
    },
    "sdl3": {
        "recipe_patches": ["patches/sdl3/recipe.patch"],
    },
}
