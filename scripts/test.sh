#!/usr/bin/env bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/lib.sh"

# every test binary runs under the memory guard: the executor wraps native runs
# in $DEBUGGER, cross builds keep their own emulator (wine/qemu) untouched
export DEBUGGER="$HERE/test_guard.sh"

echo "Compiling tests..."

ACTION=(make itest)

export brief=1
export lua=0
export py=0
# imgui links shared only: static runs leave it out
imgui=""
if [[ "${BUILD_STATIC:-0}" != "1" ]]; then
    imgui=",imgui"
fi

# native configs run every module's tests
export m="json,util,sys,core,csv,zip,net,curl,http,usb${imgui}"

build_target "clang" compiler=clang
# the terminal tests drive the scenes binary the clang build just made
if [[ -n "$imgui" ]] && ! python3 -c 'import pytest, pyte' 2>/dev/null; then
    echo "Skipping imgui terminal tests: python3 lacks pytest or pyte"
elif [[ -n "$imgui" ]]; then
    echo "Running imgui terminal tests..."
    _record "imgui-tui" env SIHD_IMGUI_SCENES_BIN="$REPO_ROOT/build/last/bin/sihd_imgui_ncurses_scenes" \
        timeout 300 python3 -m pytest -q -p no:cacheprovider "$REPO_ROOT/modules/imgui/test/tui"
fi
build_target "gcc-musl" libc=musl mode=size

# http tests deadlock under emulated runtimes (qemu-user, wine): run them on
# native configs only. curl tests are file:// based and qemu-safe.
export m="json,util,sys,core,csv,zip,net,curl,usb${imgui}"

build_target "aarch64" machine=aarch64
build_target "riscv64" machine=riscv64
# net runs on windows too since the DeviceTcpClient rx drain got a recv
# timeout: wine never completes a pending recv async on shutdown, the old
# shutdown-unblocks-blocking-recv stop path parked both threads forever
build_target "windows" platform=windows

# arm32 has no curl extlib from vcpkg
export m="json,util,sys,core,csv,zip,net"

build_target "arm32" machine=arm32

summary "tests build results"
