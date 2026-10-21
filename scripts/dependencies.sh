#!/usr/bin/env bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/lib.sh"

echo "Compiling dependencies..."

ACTION=(make dep)

export lua=1
export py=1
export demo=1
export test=1

default_matrix

summary "dependencies build results"
