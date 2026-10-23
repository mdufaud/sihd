#!/usr/bin/env bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/lib.sh"

export DEBUGGER="$HERE/test_guard.sh"

echo "Compiling benches..."

ACTION=(make bench)

export lua=0
export py=0
export m="json,util,core,sys,http"

# timings only mean something native
build_target "gcc"

summary "benches results"
