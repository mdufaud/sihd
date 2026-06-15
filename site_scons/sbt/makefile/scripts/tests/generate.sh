#!/usr/bin/env bash
# Generates <build>/test/.env (paths + cross-test runner vars) and installs the
# static executor.sh as <build>/test/execute_tests.sh.
# Required env: SBT_ENV_JSON.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

eval "$(jq -r '
    "APP_NAME=\(.app_name | @sh)",
    "PROJECT_ROOT_PATH=\(.root_path | @sh)",
    "BUILD_PATH=\(.build_path | @sh)",
    "LIB_PATH=\("\(.build_path)/\(.paths.lib)" | @sh)",
    "BIN_PATH=\("\(.build_path)/\(.paths.bin)" | @sh)",
    "ETC_PATH=\("\(.build_path)/\(.paths.etc)" | @sh)",
    "SHARE_PATH=\("\(.build_path)/\(.paths.share)" | @sh)",
    "EXTLIB_PATH=\("\(.build_path)/\(.paths.extlib)" | @sh)",
    "EXTLIB_LIB_PATH=\("\(.build_path)/\(.paths.extlib_lib)" | @sh)",
    "TEST_PATH=\("\(.build_path)/\(.paths.test)" | @sh)",
    "TEST_BIN_PATH=\("\(.build_path)/\(.paths.test_bin)" | @sh)",
    "TEST_RUNNER=\(.test.runner | @sh)",
    "TEST_BIN_EXT=\(.test.bin_ext | @sh)"
    ' "$SBT_ENV_JSON")"

env_file="$TEST_PATH/.env"

{
    printf 'export APP_NAME=%q\n' "$APP_NAME"
    printf 'export TEST_PATH=%q\n' "$TEST_PATH"
    printf 'export TEST_BIN_PATH=%q\n' "$TEST_BIN_PATH"
    printf 'export PROJECT_ROOT_PATH=%q\n' "$PROJECT_ROOT_PATH"
    printf 'export BUILD_PATH=%q\n' "$BUILD_PATH"
    printf 'export LIB_PATH=%q\n' "$LIB_PATH"
    printf 'export BIN_PATH=%q\n' "$BIN_PATH"
    printf 'export ETC_PATH=%q\n' "$ETC_PATH"
    printf 'export SHARE_PATH=%q\n' "$SHARE_PATH"
    printf 'export EXTLIB_PATH=%q\n' "$EXTLIB_PATH"
    printf 'export EXTLIB_LIB_PATH=%q\n' "$EXTLIB_LIB_PATH"
    printf 'export PYTHONHOME=%q\n' "$EXTLIB_PATH"
    printf 'export TEST_RUNNER=%q\n' "$TEST_RUNNER"
    printf 'export TEST_BIN_EXT=%q\n' "$TEST_BIN_EXT"
} > "$env_file"

# Cross-test runner env (WINEPATH / QEMU_*) as quoted exports.
while IFS= read -r line; do
    [ -z "$line" ] && continue
    key="${line%%=*}"
    val="${line#*=}"
    printf 'export %s=%q\n' "$key" "$val" >> "$env_file"
done < <(jq -r '.test.runner_env | to_entries[] | "\(.key)=\(.value)"' "$SBT_ENV_JSON")

cp "$SCRIPT_DIR/executor.sh" "$TEST_PATH/execute_tests.sh"
chmod +x "$TEST_PATH/execute_tests.sh"

printf 'generated test executor: %s\n' "$TEST_PATH/execute_tests.sh"
