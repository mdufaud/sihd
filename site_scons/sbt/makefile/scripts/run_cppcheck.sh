#!/bin/bash
# Usage: run_cppcheck.sh <BUILD_PATH> <PROJECT_ROOT_PATH> [module1 module2 ...]
#
# Runs cppcheck on the specified modules (or all if none given) and emits:
#   <BUILD_PATH>/cppcheck/<module>/report.xml      (cppcheck XML v2)
#   <BUILD_PATH>/cppcheck/<module>/report.txt      (human-readable summary)
#   <BUILD_PATH>/cppcheck/<module>/html/index.html (HTML report, if cppcheck-htmlreport available)
#
# Exit code is non-zero if any module has errors.

set -e

if ! command -v cppcheck >/dev/null 2>&1; then
    echo "cppcheck not found - install with your package manager" >&2
    exit 1
fi

HAVE_HTML_REPORT=0
if command -v cppcheck-htmlreport >/dev/null 2>&1; then
    HAVE_HTML_REPORT=1
fi

BUILD_PATH="$1"
PROJECT_ROOT_PATH="$2"
shift 2
MODULES="$*"

if [ -z "$BUILD_PATH" ] || [ -z "$PROJECT_ROOT_PATH" ]; then
    echo "Usage: $0 <BUILD_PATH> <PROJECT_ROOT_PATH> [module ...]" >&2
    exit 1
fi

CPPCHECK_DIR="$BUILD_PATH/cppcheck"
mkdir -p "$CPPCHECK_DIR"

# If no modules given, discover all that have a scons.py
if [ -z "$(printf '%s' "$MODULES" | tr -d ' ')" ]; then
    MODULES=$(cd "$PROJECT_ROOT_PATH" && ls -d */scons.py 2>/dev/null | sed 's|/scons.py||' | tr '\n' ' ')
fi

rc=0

for mod in $MODULES; do
    src_dir="$PROJECT_ROOT_PATH/$mod/src"
    inc_dir="$PROJECT_ROOT_PATH/$mod/include"
    [ -d "$src_dir" ] || continue

    mod_dir="$CPPCHECK_DIR/$mod"
    mkdir -p "$mod_dir"

    echo "Running cppcheck on module: $mod"

    cppcheck \
        --enable=warning,style,performance,portability \
        --std=c++20 \
        --language=c++ \
        --inline-suppr \
        --quiet \
        --error-exitcode=1 \
        --suppress=missingInclude \
        --suppress=missingIncludeSystem \
        --suppress=unusedFunction \
        --suppress=unusedStructMember \
        --xml \
        -I "$inc_dir" \
        "$src_dir" \
        2> "$mod_dir/report.xml" \
        1> "$mod_dir/report.txt" \
        || rc=$?

    # Count issues from XML
    issues=$(grep -c '<error ' "$mod_dir/report.xml" 2>/dev/null) || issues=0
    if [ "$issues" -gt 0 ]; then
        echo "Result: $issues issue(s)" >&2
    else
        echo "Result: ok"
    fi

    # Generate HTML report if cppcheck-htmlreport is available
    if [ "$HAVE_HTML_REPORT" -eq 1 ]; then
        html_dir="$mod_dir/html"
        mkdir -p "$html_dir"
        cppcheck-htmlreport \
            --title "$mod" \
            --file "$mod_dir/report.xml" \
            --report-dir "$html_dir" \
            --source-dir "$PROJECT_ROOT_PATH" \
            2>/dev/null
    fi
done

# --- Global report: merge all errors (severity=error) across modules ---
global_html_dir="$CPPCHECK_DIR/html"
if [ "$HAVE_HTML_REPORT" -eq 1 ] && command -v python3 >/dev/null 2>&1; then
    xml_files=()
    for mod in $MODULES; do
        f="$CPPCHECK_DIR/$mod/report.xml"
        [ -f "$f" ] && xml_files+=("$f")
    done
    if [ "${#xml_files[@]}" -gt 0 ]; then
        global_xml="$CPPCHECK_DIR/global_errors.xml"
        python3 "$(dirname "$0")/merge_cppcheck_xml.py" "$global_xml" "${xml_files[@]}"
        mkdir -p "$global_html_dir"
        cppcheck-htmlreport \
            --title "all modules - errors" \
            --file "$global_xml" \
            --report-dir "$global_html_dir" \
            --source-dir "$PROJECT_ROOT_PATH" \
            2>/dev/null
    fi
fi

echo ""
echo "Cppcheck reports:"
if [ -f "$global_html_dir/index.html" ]; then
    echo "- global: $global_html_dir/index.html"
fi
for mod in $MODULES; do
    mod_dir="$CPPCHECK_DIR/$mod"
    [ -d "$mod_dir" ] || continue
    if [ "$HAVE_HTML_REPORT" -eq 1 ] && [ -f "$mod_dir/html/index.html" ]; then
        echo "  - $mod: $mod_dir/html/index.html"
    elif [ -f "$mod_dir/report.xml" ]; then
        echo "  - $mod: $mod_dir/report.xml"
    fi
done

exit $rc
