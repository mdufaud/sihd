#!/usr/bin/env bash
# Fetches the cross toolchains, test runners and Android SDK sihd builds with, without root,
# into one prefix, then prints the shell lines that put them on PATH (see docs/toolchains.md).
#
# Usage:
#   scripts/setup/toolchains.sh [--prefix <dir>] [<target>...]
#   targets: aarch64 arm32 riscv64 riscv32 musl windows web qemu android (default: all)
#   prefix:  $SIHD_TOOLCHAINS, else ~/.local/share/sihd-toolchains
set -euo pipefail

ARM_VER="15.2.rel1"
ARM_URL="https://developer.arm.com/-/media/Files/downloads/gnu/$ARM_VER/binrel"
RISCV_URL="https://github.com/riscv-collab/riscv-gnu-toolchain/releases/download/2026.10.29"
MUSL_NAME="x86-64--musl--stable-2026.08-1"
MUSL_URL="https://toolchains.bootlin.com/downloads/releases/toolchains/x86-64/tarballs/$MUSL_NAME.tar.xz"
EMSDK_VER="6.0.13"
JDK_URL="https://api.adoptium.net/v3/binary/latest/17/ga/linux/x64/jdk/hotspot/normal/eclipse"
GRADLE_VER="9.8.1"
CMDLINE_TOOLS_URL="https://dl.google.com/android/repository/commandlinetools-linux-15859902_latest.zip"
NDK_VER="28.0.13004108"
ANDROID_PACKAGES=("platform-tools" "platforms;android-36" "build-tools;36.0.0" "ndk;$NDK_VER" "emulator"
    "system-images;android-35;google_apis;x86_64")
ALL_TARGETS=(aarch64 arm32 riscv64 riscv32 musl windows web qemu android)

PREFIX="${SIHD_TOOLCHAINS:-$HOME/.local/share/sihd-toolchains}"
targets=()
while (($# > 0)); do
    case "$1" in
        --prefix)
            PREFIX="${2:?--prefix needs a directory}"
            shift 2
            ;;
        -h | --help)
            sed '/^set -euo pipefail$/,$d' "$0"
            exit 0
            ;;
        aarch64 | arm32 | riscv64 | riscv32 | musl | windows | web | qemu | android)
            targets+=("$1")
            shift
            ;;
        *)
            printf 'Error: unknown argument %s, see --help\n' "$1" >&2
            exit 1
            ;;
    esac
done
((${#targets[@]} > 0)) || targets=("${ALL_TARGETS[@]}")

DL="$PREFIX/downloads"
BIN="$PREFIX/bin"
mkdir -p "$DL" "$BIN"
PREFIX="$(cd "$PREFIX" && pwd)"
DL="$PREFIX/downloads"
BIN="$PREFIX/bin"

fetch()
{
    local url="$1"
    local out="$DL/${2:-${url##*/}}"
    if [[ ! -f "$out" ]]; then
        curl -fSL --retry 3 -o "$out.part" -- "$url" >&2
        mv -- "$out.part" "$out"
    fi
    printf '%s\n' "$out"
}

extract_to()
{
    local archive="$1"
    local dst="$2"
    [[ -d "$dst" ]] && return 0
    rm -rf -- "$dst.tmp"
    mkdir -p "$dst.tmp"
    tar -xf "$archive" -C "$dst.tmp" --strip-components=1
    mv -- "$dst.tmp" "$dst"
}

# sbt looks tools up by its own GNU prefix: link e.g. aarch64-none-linux-gnu-gcc as aarch64-linux-gnu-gcc
link_prefix()
{
    local src_dir="$1"
    local src_prefix="$2"
    local dst_prefix="$3"
    local f
    for f in "$src_dir/$src_prefix"*; do
        ln -sfn -- "$f" "$BIN/$dst_prefix${f##*/"$src_prefix"}"
    done
}

# Arch packages run from any directory once extracted; other distros install them with their package manager
extract_pacman()
{
    local dst="$1"
    shift
    local url name
    if ! command -v pacman >/dev/null 2>&1; then
        printf 'Skipping %s: no pacman, install the distro packages listed in docs/toolchains.md\n' "$*" >&2
        return 1
    fi
    mkdir -p "$dst" "$PREFIX/.extracted"
    for url in $(pacman -Sp --print-format '%l' "$@"); do
        name="${url##*/}"
        [[ -f "$PREFIX/.extracted/$name" ]] && continue
        tar --zstd -xf "$(fetch "$url")" -C "$dst" --exclude='.PKGINFO' --exclude='.BUILDINFO' \
            --exclude='.MTREE' --exclude='.INSTALL'
        touch -- "$PREFIX/.extracted/$name"
    done
}

setup_aarch64()
{
    extract_to "$(fetch "$ARM_URL/arm-gnu-toolchain-$ARM_VER-x86_64-aarch64-none-linux-gnu.tar.xz")" \
        "$PREFIX/aarch64-none-linux-gnu"
    link_prefix "$PREFIX/aarch64-none-linux-gnu/bin" aarch64-none-linux-gnu- aarch64-linux-gnu-
}

# sbt prefers arm-none-linux-gnueabihf- for arm32 (ships static libstdc++), no link needed
setup_arm32()
{
    extract_to "$(fetch "$ARM_URL/arm-gnu-toolchain-$ARM_VER-x86_64-arm-none-linux-gnueabihf.tar.xz")" \
        "$PREFIX/arm-none-linux-gnueabihf"
    link_prefix "$PREFIX/arm-none-linux-gnueabihf/bin" arm-none-linux-gnueabihf- arm-none-linux-gnueabihf-
}

setup_riscv()
{
    local bits="$1"
    extract_to "$(fetch "$RISCV_URL/riscv$bits-glibc-ubuntu-24.04-gcc.tar.xz")" "$PREFIX/riscv$bits-unknown-linux-gnu"
    link_prefix "$PREFIX/riscv$bits-unknown-linux-gnu/bin" "riscv$bits-unknown-linux-gnu-" "riscv$bits-linux-gnu-"
}

# musl.cc stops at gcc 11, without std::expected: Bootlin's buildroot toolchain is recent
setup_musl()
{
    extract_to "$(fetch "$MUSL_URL")" "$PREFIX/$MUSL_NAME"
    "$PREFIX/$MUSL_NAME/relocate-sdk.sh" >/dev/null
    link_prefix "$PREFIX/$MUSL_NAME/bin" x86_64-buildroot-linux-musl- x86_64-linux-musl-
    local real dst
    # the buildroot wrapper derives its sysroot and .br_real from the path it is called by: exec it by its own path
    for real in "$PREFIX/$MUSL_NAME/bin/"x86_64-buildroot-linux-musl-*.br_real; do
        real="${real%.br_real}"
        dst="$BIN/x86_64-linux-musl-${real##*/x86_64-buildroot-linux-musl-}"
        rm -f -- "$dst" "$dst.br_real"
        printf '#!/bin/sh\nexec %q "$@"\n' "$real" > "$dst"
        chmod +x -- "$dst"
    done
}

# Arch mingw-w64-gcc is built with posix threads, which std::thread needs
setup_windows()
{
    extract_pacman "$PREFIX/mingw-w64" mingw-w64-gcc || return 0
    link_prefix "$PREFIX/mingw-w64/usr/bin" x86_64-w64-mingw32- x86_64-w64-mingw32-
}

setup_qemu()
{
    local q name
    extract_pacman "$PREFIX/qemu-user-static" qemu-user-static || return 0
    for q in "$PREFIX/qemu-user-static/usr/bin/"qemu-*-static; do
        name="${q##*/}"
        ln -sfn -- "$q" "$BIN/${name%-static}"
    done
}

setup_web()
{
    [[ -d "$PREFIX/emsdk" ]] || git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$PREFIX/emsdk"
    "$PREFIX/emsdk/emsdk" install "$EMSDK_VER"
    "$PREFIX/emsdk/emsdk" activate "$EMSDK_VER"
}

setup_android()
{
    local sdk="$PREFIX/android-sdk"
    extract_to "$(fetch "$JDK_URL" openjdk-17-linux-x64.tar.gz)" "$PREFIX/jdk-17"
    if [[ ! -d "$PREFIX/gradle-$GRADLE_VER" ]]; then
        unzip -q "$(fetch "https://services.gradle.org/distributions/gradle-$GRADLE_VER-bin.zip")" -d "$PREFIX"
    fi
    if [[ ! -d "$sdk/cmdline-tools/latest" ]]; then
        rm -rf -- "$sdk/cmdline-tools/cmdline-tools"
        mkdir -p "$sdk/cmdline-tools"
        unzip -q "$(fetch "$CMDLINE_TOOLS_URL")" -d "$sdk/cmdline-tools"
        mv -- "$sdk/cmdline-tools/cmdline-tools" "$sdk/cmdline-tools/latest"
    fi
    # the licenses are yours to read and accept: sdkmanager asks interactively
    JAVA_HOME="$PREFIX/jdk-17" "$sdk/cmdline-tools/latest/bin/sdkmanager" --sdk_root="$sdk" --licenses
    JAVA_HOME="$PREFIX/jdk-17" "$sdk/cmdline-tools/latest/bin/sdkmanager" --sdk_root="$sdk" "${ANDROID_PACKAGES[@]}"
}

for target in "${targets[@]}"; do
    printf '== %s\n' "$target"
    case "$target" in
        aarch64) setup_aarch64 ;;
        arm32) setup_arm32 ;;
        riscv64) setup_riscv 64 ;;
        riscv32) setup_riscv 32 ;;
        musl) setup_musl ;;
        windows) setup_windows ;;
        web) setup_web ;;
        qemu) setup_qemu ;;
        android) setup_android ;;
    esac
done

printf '\nToolchains are in %s, add this to your shell startup file:\n\n' "$PREFIX"
printf 'export PATH="%s:$PATH"\n' "$BIN"
if [[ -d "$PREFIX/emsdk" ]]; then
    printf 'EMSDK_QUIET=1 source "%s/emsdk/emsdk_env.sh"\n' "$PREFIX"
fi
if [[ -d "$PREFIX/android-sdk" ]]; then
    printf 'export JAVA_HOME="%s/jdk-17"\n' "$PREFIX"
    printf 'export ANDROID_SDK_PATH="%s/android-sdk"\n' "$PREFIX"
    printf 'export ANDROID_NDK_PATH="%s/android-sdk/ndk/%s"\n' "$PREFIX" "$NDK_VER"
    printf 'export PATH="$JAVA_HOME/bin:%s/gradle-%s/bin:$ANDROID_SDK_PATH/platform-tools:$ANDROID_SDK_PATH/emulator:$PATH"\n' \
        "$PREFIX" "$GRADLE_VER"
fi
printf '\nCheck what sbt picks with: make info machine=aarch64 (or any other target)\n'
