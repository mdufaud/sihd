import sys

###############################################################################
# compilation
###############################################################################

## general compilation parameters

flags = ['-Wall', '-Wextra', '-pipe', '-fPIC']
cxx_flags = ['-std=c++20']
static_defines = ['SIHD_STATIC']

# out-of-line atomics: 64-bit atomics on armv7, sub-word on riscv
# arch-wide link requirement (ABI), applied globally via machine option
arm32_libs = ['atomic']
riscv64_libs = ['atomic']
riscv32_libs = ['atomic']

## mode specifics
modes = ["debug", "fast", "size", "release"]

_default_flags = [
    "-D_FORTIFY_SOURCE=2",
]

debug_flags = _default_flags + ["-g", "-Og"]
fast_flags = ["-O0"]
size_flags = _default_flags + ["-Os"]
release_flags = _default_flags + ["-O3"]

## gcc specifics

gcc_flags = [
    "-Werror",
    "-fasynchronous-unwind-tables",
    "-fexceptions",
    "-fstack-protector",
    "-fstack-protector-strong",
    "-Wno-unknown-pragmas",
    # "-fconcepts-diagnostics-depth=2",/
    # hide pragma messages
    # "-ftrack-macro-expansion=0",
    # "-fno-diagnostics-show-caret",
]

gcc_size_link = ['-s']
gcc_release_link = ['-s']

gcc_link = [
    "-Wl,-z,defs",
    "-Wl,-z,now",
    "-Wl,-z,relro",
]

# Position Independent Executable for security hardening - only for binaries
gcc_dyn_bin_link = [
    "-Wl,-pie",
]

if hasattr(sys.stdout, 'isatty') and sys.stdout.isatty():
    gcc_flags.append("-fdiagnostics-color=always")

## clang specifics

clang_flags = [
    "-Werror",
    "-Wno-unused-command-line-argument",
    "-Wno-unknown-pragmas",
    # clang-22+ flags char8_t->char32_t inside vendored gtest headers
    "-Wno-character-conversion",
]
# clang uses libstdc++ by default on Linux, libc++ on macOS
# use clang-musl-libs if you need different libs for musl
clang_libs = ['stdc++']
clang_defines = [
    'LLVM_ENABLE_EH=YES',
    'LLVM_ENABLE_RTTI=ON',
]

clang_size_link = ['-s']
clang_release_link = ['-s']

## mingw specifics

mingw_flags = gcc_flags

## emscripten specifics

em_link = ["--emrun"]
em_size_link = ['--strip-debug']
em_release_link = ['--strip-debug']

## windows specifics

# _WIN64 -> activates sihd functionalities
# _WIN32_WINNT -> activates higher version of WIN functionalities (mingw)
# NTDDI_VERSION -> activates higher version of WIN functionalities (mingw)
windows_defines = [
    "_WIN64",
    "_WIN32_WINNT=0x0600",
    "NTDDI_VERSION=0x06000000",
    "_ISOC99_SOURCE",
]

## test specifics

test_extlibs = ['gtest']
test_libs = ['gtest']

## demo specifics

demo_extlibs = ['cli11']
demo_libs = ['CLI11']


def configure(app):
    # depends on app.version, set after the app module is loaded
    app.defines = [
        "SIHD_VERSION_MAJOR=" + app.version.split('.')[0],
        "SIHD_VERSION_MINOR=" + app.version.split('.')[1],
        "SIHD_VERSION_PATCH=" + app.version.split('.')[2],
    ]
