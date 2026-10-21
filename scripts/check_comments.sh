#!/usr/bin/env bash
# Reports the // lines added to C++ files, so narrating comments get caught.
# Reads a unified -U0 diff on stdin (fed by check_style.sh), exits 1 on hits.
#
# Usage:
#   git diff -U0 | scripts/check_comments.sh

awk '
/^\+\+\+ / { file = substr($2, 3); next }
/^@@ / { split($3, range, ","); line = substr(range[1], 2) - 1; next }
/^\+/ {
    line++
    text = substr($0, 2)
    if (text ~ /\/\// && text !~ /"[^"]*\/\/[^"]*"/ && text !~ /^\}[[:space:]]*\/\/ namespace/) {
        rule = "comment (keep only traps/invariants the code cannot show, one line)"
        printf "%s\t%s:%d: %s\n", rule, file, line, text
        hits++
    }
}
END { exit hits > 0 }
'
