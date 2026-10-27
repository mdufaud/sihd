name = 'sihd'
version = "0.1.0"
git_url = "https://github.com/mdufaud/sihd.git"

modules_path = "modules"

# files to include into this module (paths relative to site_scons/)
includes = [
    "addon/vcpkg/config.py",
    "addon/distribution.py",
    "addon/extlibs.py",
    "addon/compilation.py",
    "addon/test.py",
]

# mingw gcc-13: libstdc++ tinfo.o clashes with TU comdats of type_info::operator==
windows_static_link = ['-Wl,--allow-multiple-definition']

modules = {
    "pch": {
        "extlibs": ['fmt'],
    },
    "json": {
        "depends": ['pch'],
        "extlibs": ['simdjson'],
        "export-libs": ['simdjson'],
        # emscripten: build with atomics so objects are link-compatible
        "web-flags": ["-pthread"],
        "export-web-flags": ["-pthread"],
    },
    "util": {
        "depends": ['json'],
        "linux-libs": ['pthread'], # bionic has threading in libc
        "windows-libs": ['pthread'],
        "extlibs": ['fmt', 'cli11', 'pcre2'],
        "libs": ['CLI11', 'pcre2-8'], # vcpkg builds cli11 as a static lib
        "export-libs": ['fmt', 'CLI11', 'pcre2-8'],
        "android-libs": ['log'],
        "web-flags": [
            "-Wno-deprecated-literal-operator",
            "-Wno-unknown-pragmas",
            "-Wno-deprecated-declarations",
        ],
        "web-link": [
            "-sFORCE_FILESYSTEM",
        ],
        "export-all-flags": True,
    },
    "sys": {
        "depends": ['util'],
        "env": {
            "x11": 0,
            "wayland": 0,
        },
        "linux-extlibs": ['libuuid', 'libcap', 'dbus'],
        "linux-libs": ['dl', 'uuid', 'cap', 'dbus-1'],
        "android-extlibs": ['libuuid'],
        "android-libs": ['log', 'android', 'uuid'],
        "windows-libs": [
            'rpcrt4',
            'psapi',
            'ssp',
            'ws2_32',
            'gdi32',
            'user32',
            'imagehlp',
            'advapi32',
            'shell32',
            'ole32',
            'uuid',
            'powrprof',
            'wtsapi32',
            'dbghelp',
            # winrt activation for desktop notifications (RoGetActivationFactory, WindowsCreateStringReference)
            'runtimeobject',
            # UrlCreateFromPathW for toast images
            'shlwapi',
            # ! never add libucrt with mingw
        ],
        "export-all-libs": True,
    },
    "core": {
        "depends": ['util'],
    },
    "crypto": {
        "depends": ['sys'],
        "extlibs": ['openssl'],
        "export-libs": ['ssl', 'crypto'],
        "export-windows-libs": ['crypt32', 'bcrypt', 'ws2_32', 'mincore'],
    },
    "curl": {
        # no curl port for web/android
        "exclude-platforms": ["web", "android"],
        "depends": ['util'],
        # openssl/zlib come transitively from the curl port
        "extlibs": ['curl'],
        # shared libcurl resolves its transitive deps internally; the archive does not
        "linux-libs": ["curl"],
        "linux-static-libs": ["ssl", "crypto", "z"],
        "export-linux-libs": ["curl"],
        "export-linux-static-libs": ["ssl", "crypto", "z"],
        "export-windows-static-libs": [
            'curl', 'zlib', 'winmm', 'advapi32', 'ws2_32', 'iphlpapi', 'secur32',
            'ssl', 'crypto', 'crypt32', 'bcrypt',
        ],
        # order matters: dependencies after dependents
        "windows-static-libs": [
            'curl',
            'zlib',     # vcpkg installs libzlib.a on mingw
            'winmm',
            'advapi32',
            'ws2_32',
            'iphlpapi',
            'secur32',
            'ssl',      # mingw uses openssl, not schannel
            'crypto',
            'crypt32',
            'bcrypt',
        ],
        "windows-shared-libs": ['curl'],
    },
    "net": {
        # emscripten proxies neither poll() nor raw sockets
        "exclude-platforms": ["web"],
        "depends": ['core', 'crypto'],
        "windows-libs": ['iphlpapi'],
        "export-windows-libs": ['iphlpapi'],
    },
    "http": {
        # no libwebsockets port for web/android
        "exclude-platforms": ["web", "android"],
        "depends": ['curl', 'net'],
        "extlibs": [
            'libwebsockets',
            'zlib',
            'libuv',
            'openssl',
        ],
        "linux-extlibs": ["libcap"],
        "linux-libs": ["websockets", "uv", "cap"],
        # an .a records no deps: consumers link websockets/uv themselves
        "export-linux-libs": ["websockets", "uv"],
        "export-windows-static-libs": [
            'websockets_static',
            'pthread',
            'uv',
            'zlib',
            'winmm',
            'iphlpapi',
            'userenv',
            'advapi32',
            'user32',
            'dbghelp',
            'ole32',
            'uuid',
            'ssl',
            'crypto',
            'crypt32',
            'bcrypt',
        ],
        # order matters: dependencies after dependents
        "windows-static-libs": [
            'websockets_static', # vcpkg builds libwebsockets_static.a on mingw
            'pthread',
            'uv',
            'zlib',
            'winmm',
            'iphlpapi',
            'userenv',
            'advapi32',
            'user32',
            'dbghelp',
            'ole32',
            'uuid',
            'ssl',
            'crypto',
            'crypt32',
            'bcrypt',
        ],
        "windows-shared-libs": ['websockets'],
    },
    "pcap": {
        "exclude-platforms": ["android"],
        "depends": ['net'],
        "extlibs": ['libpcap'],
        "libs": ['pcap'],
        "parse-configs": [
            "pcap-config --cflags --libs",
        ],
    },
    "zip": {
        "depends": ['sys'],
        "extlibs": ['libzip'],
        "libs": ['zip'],
        "windows-static-libs": [
            'zlib',
            'bz2',
            'bcrypt',
        ],
        "web-libs": [
            'z',
            'bz2',
            'crypto',
        ],
        "linux-static-libs": ['z', 'bz2', 'crypto'],
        "export-linux-libs": ['zip'],
        "export-linux-static-libs": ['z', 'bz2', 'crypto'],
        "export-windows-static-libs": ['zip', 'zlib', 'bz2', 'bcrypt'],
    },
    "tui": {
        "exclude-platforms": ["android"],
        "depends": ['sys'],
        "extlibs": ['ftxui'],
        "libs": ["ftxui-component", "ftxui-dom", "ftxui-screen"],
    },
    "ssh": {
        "exclude-platforms": ["android"],
        "depends": ['sys'],
        "extlibs": ["libssh"],
        "libs": ['ssh'],
        "linux-static-libs": ['ssl', 'crypto'],
        "windows-static-libs": [
            'ssh',
            'pthread',
            'ssl',
            'crypto',
            'crypt32',
            'bcrypt',
            'iphlpapi',
        ],
    },
    "usb": {
        "exclude-platforms": ["android", "web"],
        "depends": ['sys'],
        "extlibs": ['libusb'],
        # cross linux + windows: vcpkg libusb
        "cross-libs": ['usb-1.0'],
        "parse-configs": [
            "pkg-config libusb-1.0 --cflags --libs",
        ],
    },
    "bt": {
        "allow-platforms": ["linux", "windows"],
        "depends": ['sys'],
        # winrt activation (RoInitialize, RoActivateInstance, WindowsCreateString)
        "windows-libs": ['runtimeobject'],
    },
    "wifi": {
        "allow-platforms": ["linux", "windows"],
        # wlanapi is loaded at runtime, iphlpapi comes exported by net
        "depends": ['net'],
    },
    "csv": {
        "depends": ['sys', 'core'],
    },
    "imgui": {
        "depends": ['sys'],
        # pulls libGL/libGLEW (no static archive) + dlopen GL loader: can't link static EXCEPT android.
        "allow-link-shared": True,
        "android-allow-link-shared": False,
        "extlibs": ['imgui', 'opengl'],
        # libxcrypt: autotools, no windows/web port
        "linux-extlibs": ['ncurses', 'libxcrypt'],
        "libs": ["imgui"],
        "linux-libs": ['glfw', 'SDL3', 'ncursesw'],
        # native linux: system GL/GLEW; cross linux: vcpkg glfw (imgui loads GL itself, GLEW unused)
        "linux-native-libs": ['GLEW', 'GL'],
        "linux-cross-libs": ['glfw'],
        "android-libs": ['android', 'EGL', 'GLESv3', 'log'],
        "android-defines": ['IMGUI_IMPL_OPENGL_ES3'],
        "android-pch": False,
        "windows-link": [
            "-mwindows"
        ],
        "windows-libs": [
            "glfw3",
            "opengl32",
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
            "dwmapi",
            "d3d11",
            "d3dcompiler",
            "dxgi",
        ],
        "web-libs": ['SDL3'],
        "web-flags": [
            "-pthread",
            "-Wno-deprecated-literal-operator",
            "-Wno-unknown-pragmas",
            "-Wno-deprecated-declarations",
            "-sDISABLE_EXCEPTION_CATCHING=1",
        ],
        "web-link": [
            "-sUSE_PTHREADS=1",
            "-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency",
            "-sWASM=1",
            "-sALLOW_MEMORY_GROWTH=1",
        ],
        "web-defines": ["IMGUI_IMPL_OPENGL_ES2"],
    },
}

# conditional modules - activated only if compiled explicitly or by env variable
conditional_modules = {
    "lua": {
        "conditional-env": "lua",
        "extlibs": ['lua', 'luabridge3'],
        "depends": ['util'],
        "conditional-depends": ['sys', 'core', 'json', 'csv', 'zip', 'net', 'wifi', 'http'],
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
        "depends": [],
        "conditional-depends": ['sys', 'core', 'net', 'wifi', 'http'],
        "libs": ["python3.12"],
        "flags": ['-U_FORTIFY_SOURCE', '-Wno-cpp'], # pybind11 requires optimization: FORTIFY breaks -O0
        "clang-flags": ["-Wno-unused-command-line-argument"],
    }
}

cppcheck = {
    "std": "c++23",
    "undefines": ["SIHD_LOGGING_OFF"],
}

# max severity="error" findings tolerated; None = fail on any finding
cppcheck_fail_under = 0

# minimum coverage percent for make covtest; None = no gate
gcov_fail_under = 80
gcov_fail_under_branch = 70

# False drops per-module gtest xml once merged into test/report.xml
test_module_reports = False

# seconds per test binary on non-interactive runs (itest, iasantest...): none takes 2 minutes, even emulated
test_timeout = 120


def __get_python_libname():
    import subprocess
    # python3-config reports the SYSTEM python suffix, we link vcpkg's python
    major_minor = "312"
    try:
        proc = subprocess.Popen("python3-config --extension-suffix",
                                shell=True, stdout=subprocess.PIPE)
        suffix = proc.stdout.read().decode().strip()
        if suffix:
            import re
            suffix = re.sub(r'cpython-\d+', 'cpython-{}'.format(major_minor), suffix)
            return suffix
    except Exception:
        pass
    return ""


def on_build_success(build_modules, builder):
    if "py" not in build_modules:
        return
    # importable by the interpreter only as sihd.cpython-VER-ARCH.so
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
