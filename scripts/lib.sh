#!/usr/bin/env bash
# Shared base for compile.sh / dependencies.sh / test.sh / bench.sh / compile_android.sh.
#
# A consumer sets:
#   ACTION         (array)  make invocation, e.g. ACTION=(make) / (make dep) / (make itest)
#   BUILD_SHARED   (0|1)    build shared variant of each target (default 1)
#   BUILD_STATIC   (0|1)    build static variant of each target (default 0)
#   asan           (0|1)    address sanitizer on the x86 targets (default 0)
# then calls build_target / default_matrix and ends with: summary "<title>"
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# keep ccache and emscripten caches inside the repo unless already set
export CCACHE_DIR="${CCACHE_DIR:-${REPO_ROOT}/tmp/ccache}"
export EM_CACHE="${EM_CACHE:-${REPO_ROOT}/tmp/emcache}"
mkdir -p "$CCACHE_DIR" "$EM_CACHE"

# capped compile parallelism: uncapped scons jobs across the matrix can exhaust
# memory. SIHD_BUILD_J overrides it.
SIHD_BUILD_J="${SIHD_BUILD_J:-6}"

successes=()
failures=()
failed_cmds=()

# android_sign_config (site_scons/addon/compilation.py) must name a
# keystore.properties with all four properties set: sbt takes the file as a
# build input and Gradle's release signing reads it. Replace the generated one
# with a real signing config to ship release APKs.
ensure_android_sign_config() {
    local props
    props="$(python3 -c "import sys; sys.path.insert(0, '$REPO_ROOT/site_scons/addon'); import compilation; print(compilation.android_sign_config)")"
    if [[ "$props" != /* ]]; then
        props="$REPO_ROOT/$props"
    fi
    if [[ -f "$props" ]] && grep -qE '^storeFile=.+' "$props" && grep -qE '^storePassword=.+' "$props" \
        && grep -qE '^keyAlias=.+' "$props" && grep -qE '^keyPassword=.+' "$props"; then
        return 0
    fi
    if ! command -v keytool >/dev/null 2>&1; then
        echo "keytool not found - cannot generate android sign config $props" >&2
        return 1
    fi
    local dir
    dir="$(dirname "$props")"
    mkdir -p "$dir/keystores"
    # throwaway local keystore: the password is a placeholder, not a secret
    keytool -genkeypair -keystore "$dir/keystores/release.keystore" -storepass sihd-android \
        -keypass sihd-android -alias sihd-release -keyalg RSA -keysize 2048 \
        -validity 10000 -dname "CN=sihd,O=sihd,C=FR" >/dev/null 2>&1 || return 1
    printf 'storeFile=keystores/release.keystore\nstorePassword=sihd-android\nkeyAlias=sihd-release\nkeyPassword=sihd-android\n' > "$props"
    echo "Generated android sign config: $props"
}

# _repro_env -> prints exported sihd flags as "k=v " prefix for a repro command
_repro_env() {
    local v prefix=""
    for v in lua py demo test m; do
        [[ -n "${!v:-}" ]] && prefix+="${v}=${!v} "
    done
    printf '%s' "$prefix"
}

# _record <label> <cmd...>
_record() {
    local label="$1"; shift
    if "$@"; then
        echo "Done ${label}."
        successes+=("$label")
    else
        local repro
        repro="$(_repro_env)$*"
        echo "Failed ${label}."
        printf 'Reproduce: %s\n' "$repro" >&2
        failures+=("$label")
        failed_cmds+=("${label}: ${repro}")
    fi
}

# build_target <name> [make args...]
# Runs ACTION for the target, building shared (BUILD_SHARED=1, default) and/or
# static (BUILD_STATIC=1) variants. asan is x86-only: cross toolchains ship no
# libasan, and sbt rejects sanitizers for mingw, Emscripten and the NDK.
build_target() {
    local name="$1"; shift
    local -a no_asan=()
    if [[ "${asan:-0}" == "1" ]]; then
        case "$*" in
            *machine=*|*platform=*|*libc=*|*compiler=em*|*compiler=ndk*|*compiler=zig*) no_asan=(asan=0) ;;
        esac
    fi
    local -a jcap=()
    case " ${ACTION[*]} $* " in *" j="*) ;; *) jcap=("j=${SIHD_BUILD_J}") ;; esac
    if [[ "${BUILD_SHARED:-1}" == "1" ]]; then
        echo "Building ${name}..."
        _record "$name" "${ACTION[@]}" "${jcap[@]}" "${no_asan[@]}" "$@"
    fi
    if [[ "${BUILD_STATIC:-0}" == "1" ]]; then
        echo "Building ${name}-static..."
        _record "${name}-static" env static=1 "${ACTION[@]}" "${jcap[@]}" "${no_asan[@]}" "$@"
    fi
}

ANDROID_TARGET=(platform=android machine=aarch64 static=1 m=util,imgui demo=1)

# Build matrix shared by compile.sh and dependencies.sh.
# The caller exports the base flags (lua/py/demo/test) beforehand.
default_matrix() {
    build_target "gcc"
    build_target "clang" compiler=clang
    build_target "gcc-musl" libc=musl mode=size
    build_target "aarch64" machine=aarch64
    build_target "riscv64" machine=riscv64

    # release just to check it builds
    build_target "gcc-release" mode=release

    # windows: py disabled (vcpkg python3 unsupported on mingw)
    # emscripten: has no python
    # arm32: not worth it
    export py=0
    build_target "windows" platform=windows

    export lua=0
    export m="util,sys,core,csv,zip,net"
    build_target "arm32" machine=arm
    build_target "emscripten" platform=web

    # Android: only when ANDROID_NDK_PATH is set
    if [[ -n "${ANDROID_NDK_PATH:-}" ]]; then
        build_target "android-arm64" "${ANDROID_TARGET[@]}"
    fi
}

# summary <title> -> exit status: 0 when no failures
summary() {
    local title="$1"
    echo
    echo "Summary of ${title}:"
    echo "-------------------------"
    echo "Succeeded (${#successes[@]}): ${successes[*]:-none}"
    echo "Failed (${#failures[@]}): ${failures[*]:-none}"
    if [[ ${#failed_cmds[@]} -gt 0 ]]; then
        echo
        echo "Reproduce failures:"
        local cmd
        for cmd in "${failed_cmds[@]}"; do
            printf '  %s\n' "$cmd"
        done
    fi
    echo
    [[ ${#failures[@]} -eq 0 ]]
}
