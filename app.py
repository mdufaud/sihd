name = 'sihd'
version = "0.1.0"
git_url = "https://github.com/mdufaud/sihd.git"

# files to include into this module (paths relative to site_scons/)
includes = [
    "addon/vcpkg/config.py",
    "addon/distribution.py",
    "addon/extlibs.py",
    "addon/compilation.py",
    "addon/test.py",
]

###############################################################################
# modules
###############################################################################

modules = {
    "json": {
        "extlibs": ['simdjson'],
        "export-libs": ['simdjson'],
        # emscripten: build with atomics so objects are link-compatible
        "web-flags": ["-pthread"],
        "export-web-flags": ["-pthread"],
    },
    "util": {
        "depends": ['json'],
        "linux-libs": ['pthread'], # threading (bionic has it in libc)
        "windows-libs": ['pthread'], # mingw winpthread
        "extlibs": ['fmt'],
        "export-libs": ['fmt'],
        # stdc++fs only needed for GCC < 9 with glibc
        "mingw-gnu-libs": ['stdc++fs'],
        # === Android specific ===
        "android-libs": ['log'],
        # === Emscripten specific ===
        "web-flags": [
            "-Wno-deprecated-literal-operator",
            "-Wno-unknown-pragmas",
            "-Wno-deprecated-declarations",
        ],
        "web-link": [
            "-sFORCE_FILESYSTEM", # use filesystem
        ],
        # === Export every flags for convenience ===
        "export-all-flags": True,
    },
    "sys": {
        "depends": ['util'],
        "env": {
            "x11": 0, # x11=1 to compile with X11
            "wayland": 0, # wayland=1 to compile with Wayland
        },
        # === Linux specific ===
        "linux-extlibs": ['libuuid'],
        "linux-libs": [
            'dl', # dlopen
            'uuid', # libuuid
        ],
        # === Android specific ===
        "android-extlibs": ['libuuid'],
        "android-libs": ['log', 'android', 'uuid'],
        # === Windows specific ===
        "windows-libs": [
            'rpcrt4', # Uuid
            'psapi', # GetProcessMemoryInfo
            'ssp', # winsock (Poll)
            'ws2_32', # windows socket api (Poll)
            'gdi32', # wingdi (screenshot/clipboard)
            'imagehlp', # backtrace / SymFromAddr
            # ! never add libucrt with mingw
        ],
        # === Export every libs for convenience ===
        "export-all-libs": True,
    },
    "core": {
        "depends": ['util', 'sys'],
    },
    "net": {
        # poll()/select() not proxied by emscripten + no raw sockets/getifaddrs: net unusable on web
        "exclude-platforms": ["web"],
        "depends": ['util', 'sys', 'core'],
        "windows-libs": ['iphlpapi'],
        "export-windows-libs": ['iphlpapi'],
    },
    "http": {
        "depends": ['net'],
        "extlibs": [
            'libwebsockets',
            'curl',
            'zlib',
            'libuv',
            'openssl', # TLS for libwebsockets + curl
        ],
        "linux-extlibs": ["libcap"],
        # all libs are platform-specific due to different names and link order requirements
        # ssl/crypto last: openssl provides symbols used by websockets/curl
        "linux-libs": ["websockets", "curl", "z", "uv", "cap", "ssl", "crypto"],
        # Windows static linking: all transitive deps must be explicit
        # order matters: higher-level libs first, their deps after; ssl/crypto last
        "windows-static-libs": [
            'websockets_static', # vcpkg builds libwebsockets_static.a on mingw
            'curl',              # libcurl
            'uv',                # libuv (libwebsockets uses it)
            'zlib',              # vcpkg zlib installs libzlib.a on mingw
            'winmm',             # Multimedia (curl)
            'iphlpapi',          # IP Helper (libwebsockets, libuv)
            'userenv',           # User environment (libwebsockets, libuv)
            'advapi32',          # Advanced API (curl, libuv)
            'user32',            # User interface (libuv)
            'dbghelp',           # Debug (libuv)
            'ole32',             # COM (libuv)
            'uuid',              # UUID (libuv)
            'ssl',               # OpenSSL TLS (libwebsockets/curl) - mingw uses openssl not schannel
            'crypto',            # OpenSSL (ssl dep)
            'crypt32',           # Windows crypto (openssl backend)
            'bcrypt',            # Windows crypto (openssl backend)
        ],
        # Windows dynamic linking: import libs (.dll.a) resolve transitive deps
        # inside each DLL, so only the directly-used libs are needed
        "windows-shared-libs": [
            'websockets',        # libwebsockets.dll.a
            'curl',              # libcurl.dll.a
        ],
    },
    "pcap": {
        "depends": ['net'],
        "extlibs": ['libpcap'],
        "libs": ['pcap'],
        # if lib is installed on system
        "parse-configs": [
            "pcap-config --cflags --libs",
        ],
        # Windows static linking: libpcap needs winsock
    },
    "zip": {
        "depends": ['util', 'sys'],
        "extlibs": ['libzip'],
        "libs": ['zip'],
        # Windows static linking: libzip's transitive deps must be explicit
        # (dynamic build resolves them inside libzip.dll via the plain 'zip' lib)
        "windows-static-libs": [
            'zlib',   # vcpkg zlib installs libzlib.a on mingw (deflate/inflate/crc32)
            'bz2',    # bzip2 compression (zip_algorithm_bzip2.c)
            'bcrypt', # Windows CNG crypto (zip_crypto_win.c)
        ],
        # emscripten static linking: libzip's transitive deps must be explicit
        "web-libs": [
            'z',      # vcpkg zlib installs libz.a on wasm (deflate/inflate/crc32/zError)
            'bz2',    # bzip2 compression (zip_algorithm_bzip2.c)
            'crypto', # openssl crypto for AES/HMAC (zip_crypto_openssl.c, zip_winzip_aes.c)
        ],
    },
    "tui": {
        "depends": ['util', 'sys'],
        "extlibs": ['ftxui'],
        "libs": ["ftxui-component", "ftxui-dom", "ftxui-screen"],
    },
    "ssh": {
        "depends": ['util', 'sys'],
        "extlibs": ["libssh"],
        "libs": ['ssh'],
        # libssh.a depends on OpenSSL (needed for cross static linking)
        "linux-cross-libs": ['ssl', 'crypto'],
        # native static: libssh.a needs system OpenSSL archives after it
        "linux-native-static-libs": ['ssl', 'crypto'],
        # static: libssh.a's transitive deps must be explicit
        # (dynamic build resolves them inside libssh.dll via the plain 'ssh' lib)
        "windows-static-libs": [
            'ssh',               # libssh
            'ssl', 'crypto',     # OpenSSL (libssh dep)
            'crypt32',           # CryptoAPI (OpenSSL)
            'bcrypt',            # BCrypt (OpenSSL)
            'iphlpapi',          # if_nametoindex (libssh)
        ],
    },
    "usb": {
        "depends": ['util', 'sys'],
        "extlibs": ['libusb'],
        # native linux: udev is a system transitive dep of libusb, parse-config provides libusb flags
        "native-libs": ['udev'],
        # cross linux: vcpkg libusb built without udev, parse-configs skipped in cross
        "cross-libs": ['usb-1.0'],
        "parse-configs": [
            "pkg-config libusb-1.0 --cflags --libs",
        ],
    },
    "bt": {
        "allow-platforms": ["linux"],
        "depends": ['util', 'sys'],
        "extlibs": ['simpleble'],
        "libs": ['simpleble'],
        # dbus-1 is a transitive dep of simpleble on Linux (needed for static linking)
        "linux-libs": ['dbus-1'],
    },
    "csv": {
        "depends": ['util', 'sys'],
    },
    "imgui": {
        "depends": ['util', 'sys'],
        # pulls libGL/libGLEW (no static archive) + dlopen GL loader: can't link static
        "allow-link-shared": True,
        "extlibs": ['imgui', 'opengl'],
        # libxcrypt only supports linux|osx (autotools, no Windows/web port)
        "linux-extlibs": ['ncurses', 'libxcrypt'],
        "libs": ["imgui"],
        "linux-libs": ['glfw', 'SDL3', 'ncursesw'],
        # native linux: system provides libglfw.so, libGLEW.so, libGL.so
        "linux-native-libs": ['GLEW', 'GL'],
        # cross linux: vcpkg provides libglfw.so (dynamic) / libglfw3.a (static)
        # (imgui has its own GL loader via dlopen, GLEW is unused)
        "linux-cross-libs": ['glfw'],
        # === Android specific ===
        "android-libs": ['android', 'EGL', 'GLESv3', 'log'],
        "android-defines": ['IMGUI_IMPL_OPENGL_ES3'],
        # == Windows specific ===
        "windows-link": [
            "-mwindows" # no shell window opening
        ],
        "windows-libs": [
            "glfw3",
            "opengl32",
            # SDL NEEDED
            "SDL3",
            "kernel32",
            "user32",
            "winmm",
            "imm32",
            "ole32",
            "oleaut32",
            "version",
            "uuid",
            "advapi32",
            "setupapi",
            "shell32",
            "dinput8",
            # END
            # desktop window manager api
            "dwmapi",
            # direct x11
            "d3d11",
            # shader compiler
            "d3dcompiler",
            # directx graphics infrastructure
            "dxgi",
        ],
        # === Web/Emscripten specific ===
        "web-libs": ['SDL3'],
        "web-flags": [
            "-pthread", # enable threads
            "-Wno-deprecated-literal-operator",
            "-Wno-unknown-pragmas",
            "-Wno-deprecated-declarations",
            "-sDISABLE_EXCEPTION_CATCHING=1",
        ],
        "web-link": [
            "-sUSE_PTHREADS=1", # enable threads
            "-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency", # use max cpu threads
            "-sWASM=1",
            "-sALLOW_MEMORY_GROWTH=1", # dynamic heap growth
        ],
        "web-defines": ["IMGUI_IMPL_OPENGL_ES2"],
    },
}

# conditional modules - activated only if compiled explicitly or by env variable
conditional_modules = {
    "lua": {
        "conditional-env": "lua",
        "extlibs": ['lua', 'luabridge3'],
        "depends": ['util', 'sys'],
        "conditional-depends": ['core'],
        "libs": ["lua"],
        "flags": ["-Wno-unused-parameter", "-Wno-unused-but-set-parameter"],
        "pkg-configs": ["lua-5.3", "lua53"],
    },
    "luabin": {
        "conditional-env": "lua",
        "depends": ['lua'],
        "libs": ["lua"],
        "flags": ["-Wno-unused-parameter"],
    },
    "py": {
        "conditional-env": "py",
        "allow-platforms": ["linux"],
        "extlibs": ['pybind11', 'python3'],
        "depends": ['util', 'sys'],
        "conditional-depends": ['core'],
        "libs": ["python3.12"],
        "flags": ['-U_FORTIFY_SOURCE', '-Wno-cpp'], # Undefine _FORTIFY_SOURCE for py module since it requires optimization from builds using -O0
        "clang-flags": ["-Wno-unused-command-line-argument"],
    }
}

###############################################################################
# cppcheck
###############################################################################

cppcheck = {
    "undefines": ["SIHD_LOGGING_OFF"],
}

###############################################################################
# after build
###############################################################################

# linux extension is something like -> cpython-3Xm-x86_64-linux-gnu.so
# windows extension -> cp3X-win_amd64.pyd
# though python cannot be used with mingw


def __get_python_libname():
    import subprocess
    # Use the vcpkg Python version for the extension suffix, not the system one.
    # python3-config --extension-suffix returns the system Python suffix which may
    # differ from the vcpkg-provided Python version we actually link against.
    major_minor = "312"
    try:
        proc = subprocess.Popen("python3-config --extension-suffix",
                                shell=True, stdout=subprocess.PIPE)
        suffix = proc.stdout.read().decode().strip()
        if suffix:
            # Replace the version in the suffix (e.g. cpython-314 -> cpython-312)
            import re
            suffix = re.sub(r'cpython-\d+', 'cpython-{}'.format(major_minor), suffix)
            return suffix
    except Exception:
        pass
    return ""


def on_build_success(build_modules, builder):
    if "py" not in build_modules:
        return
    # for pybind11 shared lib to be usable by python interpreter
    # it needs to be like lib*.cpython-VER-ARCH-VENDOR-OS.so
    python_libname = __get_python_libname()
    if not python_libname:
        return
    import os
    import glob
    import shutil
    lib_pattern = os.path.join(builder.build_lib_path, "libsihd_py*.so*")
    for lib in glob.glob(lib_pattern):
        pybind11_compliant = lib.replace("libsihd_py", "sihd")
        pybind11_compliant = pybind11_compliant.replace(".so", python_libname)
        shutil.copyfile(lib, pybind11_compliant)
