#!/usr/bin/env bash
# cache_clean.sh — Diagnostic and safe cleanup of stale vcpkg/build artifacts.
#
# Usage:
#   scripts/cache_clean.sh            # diagnostic only (no changes)
#   scripts/cache_clean.sh --clean    # interactive cleanup after confirmation
#   scripts/cache_clean.sh --help     # this message
#
# What this script considers "stale":
#   1. build/<arch>/<compiler-X>/ dirs where another version of the same compiler
#      was built more recently for the same architecture (e.g. gcc-14 last built
#      before gcc-16)
#   2. .vcpkg/installed/ — dormant artifacts from direct vcpkg install
#      (SBT uses per-build vcpkg_installed/ under build/ instead)
#   3. .vcpkg/archives/ entries older than 90 days (binary cache, compiler-ABI-keyed)
#
# SAFETY: archives/ is a shared content-addressed cache. We ONLY prune entries by AGE,
# never by content. Old entries are rebuildable. Downloads and buildtrees are NEVER touched
# by the --clean mode — they are shared across all triplets and architectures.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# vcpkg root resolution — same rule as sbt (site_scons/sbt/core/builder.py):
# VCPKG_ROOT env var if set, else <repo>/.vcpkg
VCPKG_DIR="${VCPKG_ROOT:-$REPO_ROOT/.vcpkg}"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

hr() { local line; printf -v line '%80s' ''; printf '%s\n' "${line// /─}"; }

# ─── helpers ─────────────────────────────────────────────────────────────────

human() {
    # Convert bytes to human-readable (e.g. 3.6G) — pure bash, no bc
    local bytes=$1
    if (( bytes >= 1073741824 )); then
        local gb=$(( (bytes * 10) / 1073741824 ))
        printf '%d.%dG' $((gb / 10)) $((gb % 10))
    elif (( bytes >= 1048576 )); then
        local mb=$(( (bytes * 10) / 1048576 ))
        printf '%d.%dM' $((mb / 10)) $((mb % 10))
    elif (( bytes >= 1024 )); then
        local kb=$(( (bytes * 10) / 1024 ))
        printf '%d.%dK' $((kb / 10)) $((kb % 10))
    else
        echo "${bytes}B"
    fi
}

confirm() {
    local prompt="$1"
    local answer
    read -r -p "$prompt [y/N] " answer
    [[ "$answer" =~ ^[Yy]$ ]]
}

# ─── compiler dir names ──────────────────────────────────────────────────────

compiler_version_of_dirname() {
    # "gcc-16" → "16", "clang-22" → "22", "mingw-16" → "16"
    local name="$1"
    echo "$name" | sed -n 's/^.*-\([0-9]\+\)$/\1/p'
}

compiler_family_of_dirname() {
    # "gcc-16" → "gcc", "clang-22" → "clang", "mingw-16" → "mingw"
    local name="$1"
    echo "$name" | sed 's/-[0-9]\+$//'
}

# ─── diagnostic ──────────────────────────────────────────────────────────────

diag_build_dirs() {
    echo -e "${BOLD}═══ build/ — per-architecture build directories ═══${NC}"
    echo ""

    echo -e "${CYAN}Scanning build/<arch>/<compiler-version>/ ...${NC}"
    echo ""

    # Phase 1: collect all dirs and the most recent build time per arch/compiler-family.
    # The version number is not the criterion: a cross toolchain can be older than
    # an unused build dir of the same family (e.g. mingw-16 in use, mingw-23 orphaned).
    declare -A latest_mtime
    declare -a all_dirs
    local dir_count=0

    while IFS= read -r -d '' dir; do
        local rel="${dir#$REPO_ROOT/}"
        local dirname arch comp_family comp_ver bytes size mtime
        dirname="$(basename "$dir")"
        arch="$(echo "$rel" | cut -d/ -f2)"
        comp_family="$(compiler_family_of_dirname "$dirname")"
        comp_ver="$(compiler_version_of_dirname "$dirname")"
        bytes=$(du -sb "$dir" 2>/dev/null | cut -f1)
        size="$(human "$bytes")"
        mtime=$(find "$dir" -type f -printf '%T@\n' 2>/dev/null | sort -n | tail -1)
        mtime="${mtime%.*}"
        mtime="${mtime:-0}"

        local key="$arch/$comp_family"
        (( mtime > ${latest_mtime[$key]:-0} )) && latest_mtime["$key"]=$mtime
        all_dirs+=("${comp_ver}|${dir}|${bytes}|${size}|${rel}|${mtime}")
        dir_count=$((dir_count + 1))
    done < <(find "$REPO_ROOT/build" -maxdepth 3 -type d \( -name 'gcc-*' -o -name 'clang-*' -o -name 'mingw-*' \) -print0 2>/dev/null)

    if (( dir_count == 0 )); then
        echo "  No build directories found."
        STALE_BUILD_DIRS=()
        STALE_BUILD_SIZE=0
        return
    fi

    # Phase 2: display and classify
    local stale_total=0
    local stale_count=0
    declare -a stale_paths=()
    local last_arch=""

    # Sort all_dirs for display: by arch then compiler version desc
    local sorted
    sorted=$(printf '%s\n' "${all_dirs[@]}" | sort -t'|' -k5,5 -k1,1nr)

    while IFS='|' read -r ver dir bytes size rel mtime; do
        [[ -z "$rel" ]] && continue
        local arch="${rel#build/}"; arch="${arch%%/*}"
        local comp_family dirname
        dirname="$(basename "$dir")"
        comp_family="$(compiler_family_of_dirname "$dirname")"
        local key="$arch/$comp_family"

        # Section header per architecture
        if [[ "$arch" != "$last_arch" ]]; then
            echo -e "  ${BOLD}${arch}/${NC}"
            last_arch="$arch"
        fi

        local status=""
        local stale=0

        if (( mtime < ${latest_mtime[$key]} )); then
            status="${RED}⚠  STALE${NC}  "
            stale=1
        else
            status="${GREEN}✓  LATEST${NC} "
        fi

        printf "    %b  %-20s  %-8s  last built %s\n" "$status" "$comp_family-$ver" "$size" "$(date -d @"$mtime" '+%Y-%m-%d')"

        if (( stale )); then
            stale_total=$((stale_total + bytes))
            stale_count=$((stale_count + 1))
            stale_paths+=("$dir")
        fi
    done <<< "$sorted"

    echo ""

    if (( stale_count > 0 )); then
        echo -e "${YELLOW}Found $stale_count stale build dir(s) — $(human $stale_total) reclaimable${NC}"
        STALE_BUILD_DIRS=("${stale_paths[@]}")
        STALE_BUILD_SIZE=$stale_total
    else
        echo -e "${GREEN}No stale build directories found (only the most recently built per arch/compiler).${NC}"
        STALE_BUILD_DIRS=()
        STALE_BUILD_SIZE=0
    fi
}

diag_vcpkg_installed() {
    echo ""
    hr
    echo -e "${BOLD}═══ .vcpkg/installed/ — direct vcpkg install artifacts ═══${NC}"
    echo ""

    local installed_dir="$VCPKG_DIR/installed"

    if [[ ! -d "$installed_dir" ]]; then
        echo "  (does not exist)"
        INSTALLED_SIZE=0
        return
    fi

    local bytes size
    bytes=$(du -sb "$installed_dir" 2>/dev/null | cut -f1)
    size="$(human "$bytes")"

    echo -e "  ${YELLOW}⚠  DORMANT${NC}  .vcpkg/installed/  $size"
    echo ""
    echo "  Contents:"
    ls -1 "$installed_dir" 2>/dev/null | while read -r sub; do
        local sub_bytes sub_size
        sub_bytes=$(du -sb "$installed_dir/$sub" 2>/dev/null | cut -f1)
        sub_size="$(human "$sub_bytes")"
        printf "    %-50s %s\n" "$sub/" "$sub_size"
    done
    echo ""
    echo "  SBT uses per-build vcpkg_installed/ under build/<arch>/<compiler>/<mode>/<linkage>/vcpkg/"
    echo "  This top-level .vcpkg/installed/ is from direct 'vcpkg install' commands and is safe to remove."

    INSTALLED_SIZE=$bytes
}

diag_vcpkg_archives() {
    echo ""
    hr
    echo -e "${BOLD}═══ .vcpkg/archives/ — binary cache ═══${NC}"
    echo ""

    local archives_dir="$VCPKG_DIR/archives"
    if [[ ! -d "$archives_dir" ]]; then
        echo "  (does not exist)"
        ARCHIVES_SIZE=0
        ARCHIVES_OLD_SIZE=0
        return
    fi

    local total_bytes file_count
    total_bytes=$(du -sb "$archives_dir" 2>/dev/null | cut -f1)
    file_count=$(find "$archives_dir" -type f 2>/dev/null | wc -l)

    echo "  Total: $(human "$total_bytes") across ${file_count} files"
    echo ""

    # Show date range
    local oldest_ts newest_ts
    oldest_ts=$(set +o pipefail; find "$archives_dir" -type f -printf '%T@\n' 2>/dev/null | sort -n | head -1)
    newest_ts=$(set +o pipefail; find "$archives_dir" -type f -printf '%T@\n' 2>/dev/null | sort -n | tail -1)

    if [[ -n "$oldest_ts" && -n "$newest_ts" ]]; then
        echo "  Oldest entry: $(date -d @"${oldest_ts%.*}" '+%Y-%m-%d %H:%M')"
        echo "  Newest entry: $(date -d @"${newest_ts%.*}" '+%Y-%m-%d %H:%M')"
        echo ""
    fi

    # Count entries older than 90 days
    local cutoff_ts
    cutoff_ts=$(date -d '90 days ago' +%s)
    local old_count=0 old_bytes=0

    while IFS= read -r -d '' f; do
        local mtime
        mtime=$(stat -c '%Y' "$f" 2>/dev/null || echo 0)
        if (( mtime < cutoff_ts )); then
            old_count=$((old_count + 1))
            local fbytes
            fbytes=$(stat -c '%s' "$f" 2>/dev/null || echo 0)
            old_bytes=$((old_bytes + fbytes))
        fi
    done < <(find "$archives_dir" -type f -print0 2>/dev/null)

    if (( old_count > 0 )); then
        echo -e "  ${YELLOW}Entries older than 90 days:${NC} $old_count files, $(human "$old_bytes")"
        echo ""
        echo "  vcpkg binary cache is keyed by ABI hash (includes compiler version)."
        echo "  When the compiler upgrades (e.g. gcc15 → gcc16), old ABI-keyed archives"
        echo "  are never hit again — they just accumulate."
        echo ""
        echo "  NOTE: In --clean mode, only these old entries are pruned."
        echo "  They are rebuildable if needed."
    else
        echo -e "  ${GREEN}No entries older than 90 days.${NC}"
    fi

    ARCHIVES_SIZE=$total_bytes
    ARCHIVES_OLD_SIZE=$old_bytes
    ARCHIVES_OLD_COUNT=$old_count
    ARCHIVES_CUTOFF_TS=$cutoff_ts
}

diag_vcpkg_other() {
    echo ""
    hr
    echo -e "${BOLD}═══ Other .vcpkg/ directories (NEVER cleaned) ═══${NC}"
    echo ""

    local dirs=("buildtrees" "packages" "downloads")
    for d in "${dirs[@]}"; do
        local path="$VCPKG_DIR/$d"
        if [[ -d "$path" ]]; then
            local bytes
            bytes=$(du -sb "$path" 2>/dev/null | cut -f1)
            printf "  .vcpkg/%-20s %s\n" "$d/" "$(human "$bytes")"
        fi
    done
    echo ""
    echo "  These are shared across all triplets and architectures."
    echo "  Never cleaned automatically — use 'vcpkg x-ci-clean' if you want a full reset."
    echo "  Also: 'vcpkg install --clean-buildtrees-after-build' for per-build cleanup."
}

# ─── summary ─────────────────────────────────────────────────────────────────

diag_summary() {
    echo ""
    hr
    echo -e "${BOLD}═══ Summary ═══${NC}"
    echo ""

    local grand_total=0
    grand_total=$((STALE_BUILD_SIZE + INSTALLED_SIZE + ARCHIVES_OLD_SIZE))

    printf "  %-55s %s\n" "Stale build dirs (superseded compiler versions):" "$(human $STALE_BUILD_SIZE)"
    printf "  %-55s %s\n" ".vcpkg/installed/ (dormant):" "$(human $INSTALLED_SIZE)"
    printf "  %-55s %s\n" ".vcpkg/archives/ older than 90 days:" "$(human $ARCHIVES_OLD_SIZE)"
    echo "  ─────────────────────────────────────────────────────"
    printf "  %-55s %s\n" "Total reclaimable:" "$(human $grand_total)"
    echo ""

    if (( grand_total > 0 )); then
        echo -e "  Run ${BOLD}$0 --clean${NC} to reclaim this space interactively."
    else
        echo -e "  ${GREEN}Nothing to clean!${NC}"
    fi
}

# ─── cleanup ─────────────────────────────────────────────────────────────────

do_clean() {
    echo -e "${BOLD}${RED}═══ CLEANUP MODE ═══${NC}"
    echo ""

    local cleaned=0

    # 1. Stale build dirs
    if (( ${#STALE_BUILD_DIRS[@]} > 0 )); then
        echo -e "${YELLOW}[1/3] Stale build directories ($(human $STALE_BUILD_SIZE)):${NC}"
        for d in "${STALE_BUILD_DIRS[@]}"; do
            echo "  $d"
        done
        echo ""

        if confirm "Remove these build directories?"; then
            for d in "${STALE_BUILD_DIRS[@]}"; do
                echo "  Removing: $d"
                rm -rf "$d"
            done
            cleaned=$((cleaned + STALE_BUILD_SIZE))
            echo -e "  ${GREEN}Done.${NC}"
        else
            echo "  Skipped."
        fi
        echo ""
    else
        echo -e "${GREEN}[1/3] No stale build directories to clean.${NC}"
        echo ""
    fi

    # 2. .vcpkg/installed/
    if [[ -d "$VCPKG_DIR/installed" ]] && (( INSTALLED_SIZE > 0 )); then
        echo -e "${YELLOW}[2/3] .vcpkg/installed/ ($(human $INSTALLED_SIZE))${NC}"
        echo "  This is from direct 'vcpkg install' and is NOT used by the SBT build system."
        echo ""

        if confirm "Remove .vcpkg/installed/?"; then
            rm -rf "$VCPKG_DIR/installed"
            cleaned=$((cleaned + INSTALLED_SIZE))
            echo -e "  ${GREEN}Done.${NC}"
        else
            echo "  Skipped."
        fi
        echo ""
    else
        echo -e "${GREEN}[2/3] .vcpkg/installed/ does not exist or is empty.${NC}"
        echo ""
    fi

    # 3. Stale archives
    if (( ${ARCHIVES_OLD_COUNT:-0} > 0 )); then
        echo -e "${YELLOW}[3/3] .vcpkg/archives/ entries older than 90 days${NC}"
        echo "  $ARCHIVES_OLD_COUNT files, $(human ${ARCHIVES_OLD_SIZE:-0})"
        echo ""

        if confirm "Remove these old archive entries?"; then
            local removed=0
            while IFS= read -r -d '' f; do
                local mtime
                mtime=$(stat -c '%Y' "$f" 2>/dev/null || echo 0)
                if (( mtime < ${ARCHIVES_CUTOFF_TS:-0} )); then
                    rm -f "$f"
                    removed=$((removed + 1))
                fi
            done < <(find "$VCPKG_DIR/archives" -type f -print0 2>/dev/null)
            cleaned=$((cleaned + ${ARCHIVES_OLD_SIZE:-0}))
            echo -e "  ${GREEN}Removed $removed files.${NC}"
        else
            echo "  Skipped."
        fi
        echo ""
    else
        echo -e "${GREEN}[3/3] No old archive entries to clean.${NC}"
        echo ""
    fi

    # Clean up empty directories in archives/
    if [[ -d "$VCPKG_DIR/archives" ]]; then
        find "$VCPKG_DIR/archives" -type d -empty -delete 2>/dev/null || true
    fi

    echo -e "${BOLD}${GREEN}Cleanup complete. Reclaimed: $(human $cleaned)${NC}"
}

# ─── main ────────────────────────────────────────────────────────────────────

main() {
    cd "$REPO_ROOT"

    case "${1:-}" in
        --help|-h)
            sed '/^set -euo pipefail$/,$d' "$0"
            exit 0
            ;;
        --clean)
            # Run diagnostic first (populates all globals), then clean
            echo -e "${BOLD}cache_clean — diagnostic + cleanup${NC}"
            echo "  vcpkg root: $VCPKG_DIR"
            echo ""
            diag_build_dirs
            diag_vcpkg_installed
            diag_vcpkg_archives
            diag_vcpkg_other
            diag_summary
            do_clean
            ;;
        *)
            echo -e "${BOLD}cache_clean — diagnostic${NC}"
            echo "  vcpkg root: $VCPKG_DIR"
            echo "  (run with --clean for interactive cleanup)"
            echo ""
            diag_build_dirs
            diag_vcpkg_installed
            diag_vcpkg_archives
            diag_vcpkg_other
            diag_summary
            ;;
    esac
}

main "$@"
