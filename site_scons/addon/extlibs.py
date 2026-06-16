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
    "python3": "3.12.9",
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

vcpkg_baseline = "3a3285c4878c7f5a957202201ba41e6fdeba8db4"

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
}
