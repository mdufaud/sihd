#!/usr/bin/env bash
# Runs a command under a cgroup memory cap: ASan and python test runs can
# otherwise be OOM-killed by the kernel and take the session with them.
# SIHD_TEST_MEM_MAX overrides the cap (systemd-run size syntax, default 1G).
set -e
mem_max="${SIHD_TEST_MEM_MAX:-1G}"
exec systemd-run --user --scope -q -p "MemoryMax=$mem_max" -p MemorySwapMax=0 "$@"
