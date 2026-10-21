#!/usr/bin/env bash
# Builds the android target (make dep, then make: libs + demo APKs) after
# checking the toolchain environment.
#
# Required:
#   ANDROID_NDK_PATH   NDK root, read by sbt for the compiler and Gradle's ndkVersion
#   ANDROID_SDK_PATH   SDK root, read by sbt (handed to Gradle) and the make adb/emu targets
#   JAVA_HOME          JDK for Gradle and keytool (or java on PATH)
#   gradle             on PATH
# Optional:
#   ANDROID_API           minimum API level (sbt default 24)
#   ANDROID_NDK_HOST_TAG  NDK prebuilt host tag (sbt default from the host OS)
#
# What the maintainers build with:
#   NDK 28.0.13004108, SDK platform android-36, OpenJDK 17, Gradle 9.8.1,
#   Android Gradle Plugin 9.1.0 (pinned in site_scons/sbt/android/gradle/libs.versions.toml)
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/lib.sh"

COMPILE_SDK="$(sed -n 's/^[[:space:]]*compileSdk[[:space:]]*=[[:space:]]*\([0-9]\+\).*/\1/p' "$REPO_ROOT/site_scons/sbt/android/app/build.gradle")"

missing=()
if [[ -z "${ANDROID_NDK_PATH:-}" ]]; then
    missing+=("ANDROID_NDK_PATH not set (we use NDK 28.0.13004108)")
elif [[ ! -f "$ANDROID_NDK_PATH/source.properties" ]]; then
    missing+=("ANDROID_NDK_PATH=$ANDROID_NDK_PATH is not an NDK root (no source.properties)")
fi
if [[ -z "${ANDROID_SDK_PATH:-}" ]]; then
    missing+=("ANDROID_SDK_PATH not set (SDK root holding platforms/android-${COMPILE_SDK})")
elif [[ ! -d "$ANDROID_SDK_PATH/platforms/android-${COMPILE_SDK}" ]]; then
    missing+=("ANDROID_SDK_PATH=$ANDROID_SDK_PATH lacks platforms/android-${COMPILE_SDK} (compileSdk): sdkmanager \"platforms;android-${COMPILE_SDK}\"")
fi
if [[ -n "${JAVA_HOME:-}" ]]; then
    [[ -x "$JAVA_HOME/bin/java" ]] || missing+=("JAVA_HOME=$JAVA_HOME has no bin/java (we use OpenJDK 17)")
elif ! command -v java >/dev/null 2>&1; then
    missing+=("JAVA_HOME not set and no java on PATH (we use OpenJDK 17)")
fi
command -v gradle >/dev/null 2>&1 || missing+=("gradle not on PATH (we use Gradle 9.8.1)")

if (( ${#missing[@]} > 0 )); then
    printf 'Error: android build environment incomplete:\n' >&2
    printf '  %s\n' "${missing[@]}" >&2
    exit 1
fi

ensure_android_sign_config || exit 1

export lua=0
export py=0

ACTION=(make dep)
build_target "android-arm64-dep" "${ANDROID_TARGET[@]}"

ACTION=(make)
build_target "android-arm64" "${ANDROID_TARGET[@]}"

summary "android build results"
