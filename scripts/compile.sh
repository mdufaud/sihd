#!/usr/bin/env bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/lib.sh"

echo "Compiling..."

ACTION=(make)

# android targets stage APKs: the sign config must exist as a build input
ensure_android_sign_config || echo "warning: android sign config missing - android targets may fail"

export lua=1
export py=1
export demo=1

default_matrix

summary "build results"
