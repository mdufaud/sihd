#!/usr/bin/env bash
# Apply clang-format (repo .clang-format) in place on every project .cpp/.hpp.
# Scope: project-owned sources only (modules/, site_scons/); vendored trees
# (build/, extlib/, .vcpkg/, .git/) and the site_scons/sbt submodule are never touched.
#
# Usage:
#   scripts/format.sh            # format in place
#   scripts/format.sh --check    # report files that would change, no write
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
ROOTS=("modules" "site_scons")

command -v "$CLANG_FORMAT" >/dev/null 2>&1 || {
    printf 'Error: %s not found in PATH\n' "$CLANG_FORMAT" >&2
    exit 1
}
[[ -f "$ROOT/.clang-format" ]] || {
    printf 'Error: %s/.clang-format missing\n' "$ROOT" >&2
    exit 1
}

CHECK=0
[[ "${1:-}" == "--check" ]] && CHECK=1

mapfile -d '' -t files < <(
    find "${ROOTS[@]/#/$ROOT/}" \
        \( -path "$ROOT/site_scons/sbt" -prune \) -o \
        \( -type d \( -name build -o -name extlib -o -name .vcpkg -o -name .git \) -prune \) -o \
        \( -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 \)
)

(( ${#files[@]} > 0 )) || {
    printf 'No .cpp/.hpp files found under: %s\n' "${ROOTS[*]}" >&2
    exit 1
}

if (( CHECK )); then
    changed=()
    for f in "${files[@]}"; do
        "$CLANG_FORMAT" --style=file "$f" | diff -q "$f" - >/dev/null 2>&1 || changed+=("$f")
    done
    if (( ${#changed[@]} > 0 )); then
        printf 'Would reformat (%d):\n' "${#changed[@]}"
        printf '  %s\n' "${changed[@]#"$ROOT"/}"
        exit 1
    fi
    printf 'All %d files already formatted.\n' "${#files[@]}"
else
    "$CLANG_FORMAT" --style=file -i "${files[@]}"
    printf 'Formatted %d files.\n' "${#files[@]}"
fi
