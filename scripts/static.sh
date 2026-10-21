#!/usr/bin/env bash
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export BUILD_SHARED=0
export BUILD_STATIC=1

started_at=$(date +%s)
failed_steps=()

bash "$HERE/dependencies.sh" || failed_steps+=("dependencies")
dependencies_ended_at=$(date +%s)

bash "$HERE/compile.sh" || failed_steps+=("compile")
compile_ended_at=$(date +%s)

bash "$HERE/test.sh" || failed_steps+=("test")
test_ended_at=$(date +%s)

echo
echo "Build completed in: $((test_ended_at - started_at)) seconds"
echo "  dependencies:     $((dependencies_ended_at - started_at))s"
echo "  compile:          $((compile_ended_at - dependencies_ended_at))s"
echo "  test:             $((test_ended_at - compile_ended_at))s"

if [[ ${#failed_steps[@]} -gt 0 ]]; then
    printf 'Failed steps (%d): %s\n' "${#failed_steps[@]}" "${failed_steps[*]}"
    exit 1
else
    echo "All steps completed successfully."
fi
