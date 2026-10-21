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
# native configs run every module's tests
export m="json,util,sys,core,csv,zip,net,curl,http"

build_target "clang" compiler=clang
build_target "gcc-musl" libc=musl mode=size

# http tests deadlock under emulated runtimes (qemu-user, wine): run them on
# native configs only. curl tests are file:// based and qemu-safe.
export m="json,util,sys,core,csv,zip,net,curl"

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
