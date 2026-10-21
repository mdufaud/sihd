#!/usr/bin/env bash
# Greps the lines added since a git ref (default HEAD, untracked files included) for sihd style
# regressions. Heuristic: every hit is to be reviewed, not blindly fixed.
#
# Prints the hit count per rule; --all lists every hit under its rule.
#
# Usage:
#   scripts/check_style.sh                       # changes since HEAD, count per rule
#   scripts/check_style.sh <ref>                 # changes since <ref>
#   scripts/check_style.sh --all [<ref>]         # every hit, grouped by rule
#   scripts/check_style.sh --tree [<dir>]        # every .cpp/.hpp under <dir> (default modules), not only the diff
#   scripts/check_style.sh --complexity [<ref>]  # also scores the changed functions (clang-tidy, slow)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ALL=0
COMPLEXITY=0
TREE=0
TREE_DIR=modules
while [[ "${1:-}" == --* ]]; do
    case "$1" in
        --all)
            ALL=1
            ;;
        --complexity)
            COMPLEXITY=1
            ;;
        --tree)
            TREE=1
            if [[ -n "${2:-}" && "${2:-}" != --* ]]; then
                TREE_DIR="$2"
                shift
            fi
            ;;
        *)
            printf 'Error: unknown option %s\n' "$1" >&2
            exit 1
            ;;
    esac
    shift
done
REF="${1:-HEAD}"
COMPLEXITY_THRESHOLD="${COMPLEXITY_THRESHOLD:-25}"
WALL_LINES=15

cd "$ROOT"

if (( TREE )); then
    TREE_DIR="${TREE_DIR#"$ROOT"/}"
    if [[ "$TREE_DIR" == /* || ! -d "$TREE_DIR" ]]; then
        printf 'Error: --tree needs a directory inside %s\n' "$ROOT" >&2
        exit 1
    fi
fi

collect_diff() {
    local file
    if (( TREE )); then
        while IFS= read -r -d '' file; do
            [[ "$file" == *.cpp || "$file" == *.hpp ]] || continue
            [[ -f "$file" ]] || continue
            git diff --no-index -U0 -- /dev/null "$file" || true
        done < <(git ls-files -z --cached --others --exclude-standard -- "$TREE_DIR")
        return
    fi
    git diff -U0 "$REF" -- 'modules/*.cpp' 'modules/*.hpp'
    while IFS= read -r -d '' file; do
        git diff --no-index -U0 -- /dev/null "$file" || true
    done < <(git ls-files -z --others --exclude-standard -- 'modules/*.cpp' 'modules/*.hpp')
}

DIFF="$(mktemp)"
HITS="$(mktemp)"
trap 'rm -f -- "$DIFF" "$HITS"' EXIT
collect_diff > "$DIFF"

status=0
awk -v wall_lines="$WALL_LINES" '
function hit(rule) {
    printf "%s\t%s:%d: %s\n", rule, file, line, text
    hits++
}
/^\+\+\+ / {
    file = substr($2, 3)
    is_hpp = file ~ /\.hpp$/
    is_cpp = file ~ /\.cpp$/
    is_test = file ~ /\/test\//
    is_public = file ~ /\/include\//
    is_sys = file ~ /^modules\/sys\//
    is_json = file ~ /^modules\/json\//
    is_demo = file ~ /\/demo\//
    is_binding = file ~ /^modules\/(lua|py)\// || file ~ /CliApp\.(cpp|hpp)$/
    is_backend = file ~ /^modules\/(lua|py|imgui|tui|json)\//
    prev = ""
    prev2 = ""
    run = 0
    cont = 0
    in_else = 0
    else_depth = 0
    next
}
/^@@ / {
    split($3, range, ",")
    line = substr(range[1], 2) - 1
    prev = ""
    prev2 = ""
    run = 0
    cont = 0
    in_else = 0
    else_depth = 0
    next
}
/^\+/ {
    line++
    text = substr($0, 2)

    # a wall counts statements: the lines of a multi-line call are one
    if (text ~ /^[[:space:]]*$/ || text ~ /^[[:space:]]*#/) {
        run = 0
        cont = 0
    } else {
        if (!cont && text !~ /^[[:space:]]*[{}][;)]*[[:space:]]*$/ && ++run == wall_lines && !is_hpp)
            hit("wall of " wall_lines " statements without a blank line (split setup / action / assertions)")
        cont = (text !~ /[;{}][[:space:]]*$/ && text !~ /^[[:space:]]*\/\//)
    }
    if ((is_cpp || is_hpp) && text ~ /(^|[(,][[:space:]]*|^[[:space:]]+)(const[[:space:]]+)?(auto|int|bool|char|double|float|long|unsigned|size_t|ssize_t|u?int(8|16|32|64)_t|std::[a-z_0-9]+(<[^>]*>)?|[A-Z][A-Za-z0-9_:<>]*)[[:space:]&*]+[a-hl-z][[:space:]]*(=|\(|\{|;|:[^:])/ \
        && text !~ /for[[:space:]]*\(/ && text !~ /\[[^]]*\][[:space:]]*\(/)
        hit("one-letter name (say what it holds)")
    if (text ~ /^[[:space:]]*#[[:space:]]*else/ && else_depth == 0)
        in_else = 1
    else if (in_else && text ~ /^[[:space:]]*#[[:space:]]*if/)
        else_depth++
    else if (in_else && text ~ /^[[:space:]]*#[[:space:]]*endif/) {
        if (else_depth > 0)
            else_depth--
        else
            in_else = 0
    }
    if (in_else && text ~ /return[[:space:]]+(false|-1|""|std::nullopt|\{\})[[:space:]]*;/)
        hit("#else returning a dummy value (Result with ErrorCode::not_supported)")
    if (!is_test && !is_demo && !is_binding && text ~ /"sihd"/)
        hit("hardcoded sihd identity in library (setter, the consumer chooses)")

    if (is_hpp && !is_test) {
        if (text ~ /\)[[:space:]]*(const)?[[:space:]]*(noexcept)?[[:space:]]*(override)?[[:space:]]*\{/ \
            || (text ~ /^[[:space:]]*\{[[:space:]]*$/ && prev ~ /\)[[:space:]]*(const)?[[:space:]]*(noexcept)?[[:space:]]*(override)?[[:space:]]*$/ \
                && prev !~ /^[[:space:]]*(\}[[:space:]]*)?(if|else|while|for|switch|catch)[[:space:]]*\(/))
            hit("code in header (move the body to the .cpp)")
        if (text ~ /^[[:space:]]*inline[[:space:]]/)
            hit("inline function in header (move it to the .cpp)")
        if (text ~ /^[[:space:]]*template[[:space:]]*</)
            hit("template in header (is the template needed? a std::function keeps the body in the .cpp)")
        if (text ~ /^[[:space:]]*friend[[:space:]]/)
            hit("friend (no friend in sihd)")
        if (text ~ /(std::)?(is(space|alpha|digit|alnum|upper|lower|xdigit)|to(upper|lower))\(/ || text ~ /#[[:space:]]*include[[:space:]]*<cctype>/)
            hit("ctype helper in header (keep it in the .cpp)")
        if (text ~ /[a-z]_(uninterrupted|ex|safe)[[:space:]]*\(/)
            hit("twin function (one function per operation: fix the plain one)")
        if (text ~ /^[[:space:]]+[A-Za-z_:<>,*& ]+[[:space:]*&]+[a-z]+[A-Z][A-Za-z0-9]*[[:space:]]*\(/)
            hit("camelCase method (snake_case)")
    }
    if (is_public && text ~ /^[[:space:]]*#[[:space:]]*if/ && text !~ /__SIHD_[A-Z0-9_]+_HPP__/)
        hit("#if in public header (same api everywhere, return ErrorCode::not_supported)")
    if (is_public && text ~ /#[[:space:]]*include[[:space:]]+"/)
        hit("quoted include in public header (<sihd/...>)")
    if (is_public && is_hpp && !is_backend && text ~ /#[[:space:]]*include[[:space:]]*</) {
        target = text
        sub(/^[^<]*</, "", target)
        sub(/>.*$/, "", target)
        if (target ~ /^(windows|winsock|ws2|mswsock|tchar\.h|io\.h|TargetConditionals|d3d|dxgi|Python|lua|lauxlib|pybind11|sqlite3|zlib|ncurses|curses|pcap|curl|libwebsockets|libssh|simdjson|fmt|ftxui|gtest|GL|EGL|SDL|X11|wayland|android|imgui|nlohmann|boost|openssl)/)
            hit("SDK or library header in public .hpp (pImpl: forward-declare, include it in the .cpp)")
        else if (target ~ /^(sys\/|netinet\/|arpa\/|net\/if|ifaddrs\.h|poll\.h|pthread\.h|signal\.h|unistd\.h|dlfcn\.h|fcntl\.h|termios\.h|dirent\.h|pwd\.h|grp\.h|linux\/)/)
            hit("POSIX header in public .hpp (namespace pollution: pImpl, or own type in the .cpp)")
        else if (target ~ /^(regex|iostream)$/)
            hit("heavy std header in public .hpp (regex compile cost, iostream static init: move the use to the .cpp)")
    }
    if (is_public && text ~ /std::optional<std::(pair|tuple)</)
        hit("optional<pair|tuple> in public api (named struct)")
    if (is_hpp) {
        if (text ~ /^[[:space:]]*using[[:space:]]/)
            hit("using in header (usings live in the .cpp after includes, SF.7; class-scope alias: review)")
        if (text ~ /SIHD_(NEW_)?LOGGER|SIHD_LOG\(|SIHD_UNEXPECTED_LOG\(/)
            hit("logging in header")
        if (text ~ /#[[:space:]]*pragma[[:space:]]+once/)
            hit("#pragma once (use __SIHD_<MODULE>_<CLASS>_HPP__ guard)")
        if (text ~ /SIHD_REGISTER_FACTORY\(/)
            hit("SIHD_REGISTER_FACTORY in header (lives in the class own .cpp)")
        if (text ~ /(^|[^A-Za-z0-9_])m_[a-z]/)
            hit("m_ member (_member)")
    }
    if (file == "modules/util/include/sihd/util/Error.hpp" && text ~ /^[[:space:]]+[a-z_]+[[:space:]]*(=[^,]*)?,?[[:space:]]*$/)
        hit("new ErrorCode (taxonomy closed: append only when a real consumer branches on it)")
    if (file == "modules/util/include/sihd/util/type.hpp" && text ~ /(^|[[:space:]])(using|concept|template)[[:space:]]/)
        hit("traits in type.hpp (traits.hpp)")
    if (file == "modules/sys/include/sihd/sys/program.hpp" && text ~ /pid/)
        hit("pid-targeted function in sys::program (current program only: own topical namespace)")
    if (text ~ /SIHD_FACTORY_DECL\(/)
        hit("SIHD_FACTORY_DECL (registration stays in the class own .cpp, no consumer-side decl or dispatch)")
    if (is_cpp && text ~ /sihd::util::/ && text !~ /^[[:space:]]*(#include|using|namespace)/ && text !~ /SIHD_NEW_LOGGER/)
        hit("qualified sihd::util:: in .cpp (using namespace sihd::util; after the includes)")
    if (is_cpp && text ~ /__SIHD_WINDOWS__/)
        hit("platform #if in .cpp (if constexpr (build::is_windows) when both branches compile; #if only for symbols absent on the platform)")
    if (is_cpp && text ~ /^static[[:space:]][^=]*\(/)
        hit("static free function (anonymous namespace)")
    if (is_cpp && text ~ /^[[:space:]]+(struct|enum([[:space:]]+class)?)[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*(:[^;]*)?(\{[[:space:]]*)?$/)
        hit("struct/enum declared mid-code (top anonymous namespace, before functions)")
    if (!is_test && text ~ /auto[[:space:]]+[a-z_][a-z0-9_]*[[:space:]]*=[[:space:]]*\[/)
        hit("local lambda (inline per site, or anonymous-namespace helper)")
    if ((is_cpp || is_hpp) && text ~ /^[[:space:]]*(inline[[:space:]]+)?static[[:space:]]/ && text !~ /[(]/ && text !~ /(const|constexpr|thread_local)[[:space:]]/)
        hit("static mutable state (no global/static mutable state; thread_local exempt)")
    if (is_cpp && text ~ /SIHD_NEW_LOGGER\("sihd::[A-Za-z0-9_]+::/)
        hit("logger name deeper than the module namespace (sihd::<module>)")
    if (is_json && text ~ /SIHD_UNEXPECTED|sihd\/util\//)
        hit("util in json module (raw std::expected<T, std::string>, no sihd macro)")
    if (!is_sys && text ~ /(^|[^A-Za-z0-9_])(pthread_[a-z_]+|dlopen|dlsym|dlclose|fork|execv[pe]?|waitpid|epoll_[a-z]+|WSA[A-Za-z]+|CreateThread|CreateProcess[AW]?|GetLastError)[[:space:]]*\(/)
        hit("platform call outside sihd::sys (wrap it in sys)")
    if (is_test && file ~ /(^|\/)main\.cpp$/ && text !~ /^[[:space:]]*$/ && text !~ /test_watchdog|install_test_watchdog\(\)/)
        hit("test main touched (test mains add nothing)")
    if (is_test && text ~ /"\/tmp\//)
        hit("fixed /tmp path in test (per-test dir, gtest shuffles)")
    if (text ~ /EXPECT_(TRUE|FALSE)\([^;]*read_next\(\)\)|while[[:space:]]*\([^;]*read_next\(\)\)/)
        hit("Result<bool> read_next used as bool (use value_or/has_value)")
    if (!is_test && text ~ /value_or\(""\)/)
        hit("silent fallback value_or on empty (a ctor that acquires throws)")
    if (text ~ /_level_threshold|min_level_lower|set_filters_observer|notify_filters_changed|on_filters_changed|can_pass\(|LogPreview|OwnedLogInfo|LoggerFilter\*>|(^|[^a-z_])set_level\(|last_would_block|last_error_retryable|GateRelease|GateGuard|errc_error/)
        hit("vetoed name reintroduced (removed on purpose, do not bring back)")
    if (text ~ /[a-z]_of_[a-z]/ && text !~ /out_of_/)
        hit("X_of_Y name (reads as unit conversion: X_to_Y or accessor name)")

    if (text ~ /^[[:space:]]*do[[:space:]]*\{?[[:space:]]*$/ || text ~ /^[[:space:]]*\}[[:space:]]*while[[:space:]]*\(.*\);/)
        hit("do-while")
    if (text ~ /return[[:space:]]+std::unexpected\((std::move\()?[A-Za-z_][A-Za-z0-9_.>()-]*\)?\.error\(\)\)/)
        hit("manual error propagation (SIHD_UNEXPECTED_RETURN)")
    if (text ~ /Error\([A-Za-z_][A-Za-z0-9_.>()-]*\.error\(\)\.code/ || text ~ /\.message[[:space:]]*\+?=[^=]/)
        hit("manual error context (SIHD_UNEXPECTED_RETURN_CTX)")
    if (text ~ /^[[:space:]]*SIHD_UNEXPECTED_LOG\(/ \
        && (prev ~ /^[[:space:]]*if[[:space:]]*\(!/ || (prev ~ /^[[:space:]]*\{[[:space:]]*$/ && prev2 ~ /^[[:space:]]*if[[:space:]]*\(!/)))
        hit("if (!x) SIHD_UNEXPECTED_LOG(x) (if (SIHD_UNEXPECTED_LOG(x)) return ...;)")
    if (text ~ /if[[:space:]]*\(![^)]*\)[[:space:]]*\{?[[:space:]]*SIHD_UNEXPECTED_LOG\(/)
        hit("if (!x) { SIHD_UNEXPECTED_LOG(x) (if (SIHD_UNEXPECTED_LOG(x)) return ...;)")
    if (text ~ /if[[:space:]]*\(.*![[:space:]]*SIHD_UNEXPECTED_LOG\(/)
        hit("negated SIHD_UNEXPECTED_LOG in if (the macro is true on failure)")
    if (text ~ /std::expected<.*Error>/)
        hit("std::expected<T, Error> (sihd::util::Result<T>)")
    if (text ~ /std::condition_variable/)
        hit("std::condition_variable (sihd::util::Waitable)")
    if (text ~ /SIHD_UNEXPECTED_RETURN(_CTX)?\([^,()]*\(/)
        hit("SIHD_UNEXPECTED_RETURN on a call (store the result in a named variable first)")
    if (text ~ /^[[:space:]]*throw[[:space:]]/ && prev ~ /SIHD_(LOG|UNEXPECTED_LOG)/)
        hit("log before throw (the exception message is the report)")
    if (text ~ /std::(shared_ptr|make_shared)/)
        hit("shared_ptr (non-owning/raw or unique_ptr first: shared only for real shared ownership)")
    if (text ~ /\.count\([^()]*\)[[:space:]]*(!=|>)[[:space:]]*0/)
        hit("count() as predicate (contains)")
    if (text ~ /^[[:space:]]*(static[[:space:]]+|const[[:space:]]+|constexpr[[:space:]]+)*[A-Za-z_][A-Za-z0-9_:]*(<[^;>]*>)?[[:space:]]+[a-z_][a-z0-9_]*\[[^]]*\][[:space:]]*(=|;)/ \
        && text !~ /^[[:space:]]*(return|delete|throw|co_return|else)[[:space:]]/)
        hit("raw C array (std::array)")
    else if (text ~ /[a-z0-9_)\]]\[[0-9]{2,}\]/)
        hit("magic index (named constant)")
    if (text ~ /std::format\(/)
        hit("std::format (fmt::format)")
    if (!is_test && !is_demo && text ~ /(^|[^A-Za-z0-9_:.>])(printf|fprintf|puts)[[:space:]]*\(|std::(cout|cerr|endl)/)
        hit("stdio output in library (SIHD_LOG)")
    if (text ~ /(^|[^A-Za-z0-9_])typedef[[:space:]]/)
        hit("typedef (using X = ...)")
    if (text ~ /^namespace[[:space:]]+sihd[[:space:]]*(\{[[:space:]]*)?$/)
        hit("nested namespace blocks (namespace sihd::<module>)")

    prev2 = prev
    prev = text
}
!/^\+/ { run = 0 }
END { exit hits > 0 }
' "$DIFF" > "$HITS" || status=1

# native builds holding a compile database, clang ones first (clang-tidy reads them as is), newest first
native_build_dirs() {
    local host_dir="$ROOT/build/$(uname -m)-linux-gnu"
    local -a dirs
    [[ -d "$host_dir" ]] || return 1
    mapfile -t dirs < <(find "$host_dir" -maxdepth 4 -name compile_commands.json -printf '%T@ %h\n' | sort -nr | cut -d' ' -f2-)
    printf '%s\n' "${dirs[@]}" | grep '/clang-' || true
    printf '%s\n' "${dirs[@]}" | grep -v '/clang-' || true
}

# the first native build whose compile database knows the file
build_dir_of() {
    local file="$1" dir
    for dir in "${BUILD_DIRS[@]}"; do
        if grep -qF -- "\"$ROOT/$file\"" "$dir/compile_commands.json"; then
            printf '%s\n' "$dir"
            return 0
        fi
    done
    return 1
}

# a function is scored when the diff touches its warning line or one of the lines that add to its complexity
report_touched() {
    local file="$1" rel="$2" ranges="$3"
    awk -v file="$file" -v rel="$rel" -v rel_ranges="$ranges" '
    BEGIN {
        while ((getline range_line < rel_ranges) > 0) {
            split(range_line, fields, " ")
            if (fields[1] == rel) {
                count++
                low[count] = fields[2]
                high[count] = fields[3]
            }
        }
    }
    function touches(line_no,   i) {
        for (i = 1; i <= count; i++)
            if (line_no >= low[i] && line_no <= high[i])
                return 1
        return 0
    }
    function flush() {
        if (have && touched) {
            print warning
            found = 1
        }
        have = 0
        touched = 0
    }
    # clang-tidy prints the path as the compile database gave it: absolute or relative to the root
    function line_of(text,   prefix) {
        prefix = (index(text, file ":") == 1) ? file : ((index(text, rel ":") == 1) ? rel : "")
        if (prefix == "")
            return 0
        split(substr(text, length(prefix) + 2), position, ":")
        return position[1] + 0
    }
    line_of($0) > 0 && / warning: / {
        flush()
        have = 1
        warning = $0
        sub(file, rel, warning)
        touched = touches(line_of($0))
        next
    }
    have && line_of($0) > 0 && / note: / {
        if (touches(line_of($0)))
            touched = 1
        next
    }
    END { flush(); exit found }
    '
}

if (( COMPLEXITY )); then
    mapfile -t BUILD_DIRS < <(native_build_dirs)
    if ! command -v clang-tidy > /dev/null || (( ${#BUILD_DIRS[@]} == 0 )); then
        printf 'Error: --complexity needs clang-tidy and a native build with a compile database (build the changed modules with test=1 first)\n' >&2
        exit 1
    fi
    ranges="$(mktemp)"
    trap 'rm -f -- "$DIFF" "$HITS" "$ranges"' EXIT
    awk '
        /^\+\+\+ / { file = substr($2, 3) }
        /^@@ / {
            split($3, range, ",")
            start = substr(range[1], 2)
            count = (range[2] == "" ? 1 : range[2])
            if (count > 0)
                print file, start, start + count - 1
        }
    ' "$DIFF" > "$ranges"
    mapfile -t changed < <(awk '/^\+\+\+ / && $2 ~ /\.cpp$/ { print substr($2, 3) }' "$DIFF" | sort -u)
    for file in "${changed[@]}"; do
        if ! build_dir="$(build_dir_of "$file")"; then
            printf '%s: not in any native compile database, complexity not scored\n' "$file"
            continue
        fi
        if ! report="$(clang-tidy -p "$build_dir" --quiet --checks='-*,readability-function-cognitive-complexity' \
            --config="{CheckOptions: {readability-function-cognitive-complexity.Threshold: $COMPLEXITY_THRESHOLD, readability-function-cognitive-complexity.IgnoreMacros: true, readability-function-cognitive-complexity.DescribeBasicIncrements: false}}" \
            "$ROOT/$file" 2> /dev/null)"; then
            printf '%s: clang-tidy failed on %s\n' "$file" "$build_dir"
            status=1
            continue
        fi
        printf '%s\n' "$report" | report_touched "$ROOT/$file" "$file" "$ranges" || status=1
    done
fi

"$SCRIPT_DIR/check_comments.sh" < "$DIFF" >> "$HITS" || status=1

# every hit is a "rule<TAB>file:line: text" record: the summary counts the rule, --all lists the lines under it
if (( ALL )); then
    awk -F'\t' 'NR == FNR { count[$1]++; next } { print count[$1] "\t" $0 }' "$HITS" "$HITS" \
        | sort -t$'\t' -s -k1,1nr -k2,2 \
        | awk -F'\t' '
            $2 != rule { rule = $2; printf "%s (%d)\n", rule, $1 }
            { hit = $0; sub(/^[^\t]*\t[^\t]*\t/, "", hit); print "    " hit }
        '
else
    cut -f1 "$HITS" | sort | uniq -c | sort -rn
    printf '%s hits (--all for each line)\n' "$(awk 'END { print NR }' "$HITS")"
fi

exit "$status"
