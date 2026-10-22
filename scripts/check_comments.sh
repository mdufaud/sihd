#!/usr/bin/env bash
# Reports the comments added to C++ files, so narrating comments get caught.
# Reads a unified -U0 diff on stdin (fed by check_style.sh), exits 1 on hits.
#
# A one-line comment naming a trap (must, never, because, thread, lifetime...) passes, a platform name
# alone does not; banners, commented-out code, doxygen blocks and multi-line comments are always reported.
#
# Usage:
#   git diff -U0 | scripts/check_comments.sh

awk '
function report(rule) {
    printf "%s\t%s:%d: %s\n", rule, file, line, text
    hits++
}
/^\+\+\+ / { file = substr($2, 3); prev_comment = 0; next }
/^@@ / { split($3, range, ","); line = substr(range[1], 2) - 1; prev_comment = 0; next }
/^\+/ {
    line++
    text = substr($0, 2)
    if (text ~ /^[[:space:]]*\/\*[[:space:]]*[-=*#~]{3,}/) {
        report("comment banner (delete it)")
        prev_comment = 0
        next
    }
    if (text ~ /^[[:space:]]*\/\*\*/) {
        report("doxygen block (one-line // trap comment, or none)")
        prev_comment = 0
        next
    }
    if (text ~ /^[[:space:]]*\/\*/ && text !~ /\*\//) {
        report("multi-line comment (one line max)")
        prev_comment = 0
        next
    }
    # literals blanked at the same width: a "//" inside a string starts no comment
    masked = text
    while (match(masked, /"([^"\\]|\\.)*"|'\''([^'\''\\]|\\.)'\''/))
        masked = substr(masked, 1, RSTART - 1) sprintf("%" RLENGTH "s", "") substr(masked, RSTART + RLENGTH)
    start = index(masked, "//")
    block = (start == 0 && masked ~ /^[[:space:]]*\/\*.*\*\/[[:space:]]*$/)
    if (block)
        start = index(masked, "/*")
    if (start == 0 || text ~ /^\}[[:space:]]*\/\/ namespace/ \
        || text ~ /^[[:space:]]*#[[:space:]]*(endif|else)/ || text ~ /\/\/[[:space:]]*(clang-format|NOLINT|IWYU)/ \
        || text ~ /(Copyright|SPDX-License|[Ll]icen[cs]e)/) {
        prev_comment = 0
        next
    }
    whole_line = (substr(text, 1, start - 1) ~ /^[[:space:]]*$/)
    comment = substr(text, start + 2)
    if (block)
        sub(/[[:space:]]*\*\/[[:space:]]*$/, "", comment)
    sub(/^[\/[:space:]]+/, "", comment)
    lowered = tolower(comment)
    if (comment ~ /^[[:space:]]*$/ && !whole_line) {
        prev_comment = 0
        next
    }
    if (comment ~ /^[-=*#~_\/[:space:]]*$/ || comment ~ /^[-=*#~]{3,}/ || comment ~ /[-=*#~]{3,}[[:space:]]*$/)
        report("comment banner (delete it)")
    else if (comment ~ /^#[[:space:]]*include/ || comment ~ /^[{}][[:space:]]*$/ || comment ~ /^return([[:space:]].*)?;[[:space:]]*$/ \
        || (comment ~ /[;{}][[:space:]]*$/ \
            && comment ~ /^[A-Za-z_][A-Za-z0-9_:<>]*([[:space:]]*(\(|=[^=]|->|\.|\[)|[[:space:]]+[*&]?[A-Za-z_][A-Za-z0-9_]*[[:space:]]*(=|\(|;|\{))/))
        report("commented-out code (delete it)")
    else if (whole_line && prev_comment)
        report("multi-line comment (one line max)")
    # stems match at a word start only ("race" is not "backtrace"); a platform name alone is orientation, not a trap
    else if (lowered !~ /(^|[^a-z])(must|never|always|because|otherwise|unless|workaround|trap|beware|invariant|thread|gil|lifetime|ownership|owns|outlive|undefined|race|deadlock|reentr|signal|cannot|can.t|do not|don.t|required|block|guard|lost|null|ignored|silent|retr|timeout|idempot|caller|consumer|so that|order matters|not [a-z-]*safe|cve|todo|fixme)/)
        report("comment (keep only traps/invariants the code cannot show, one line)")
    prev_comment = whole_line
}
!/^\+/ { prev_comment = 0 }
END { exit hits > 0 }
'
