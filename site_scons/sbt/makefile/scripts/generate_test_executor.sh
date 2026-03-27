#!/bin/bash

executor_name=execute_tests.sh

generate_test()
(
    cd $TEST_PATH
    cat << EOF > $executor_name
#!/bin/bash
cd $TEST_PATH
source .env

log_dir="$TEST_PATH/logs"

module_list="\$2"
if [ -z "\${module_list// }" ] || [ "\${module_list// }" = "all" ]; then
    module_list=\$(cd $TEST_BIN_PATH && ls)
    module_list="\${module_list//${APP_NAME}_/}"
fi
test_filter="\$3"

print_module_failure()
(
    local module="\$1"
    local stdout_log="\${log_dir}/\${module}/stdout.log"
    local stderr_log="\${log_dir}/\${module}/stderr.log"

    echo ""
    echo "--- \${module} ---"
    grep -E "^\[  FAILED  \]" "\${stdout_log}" 2>/dev/null || true
    if [ -s "\${stderr_log}" ]; then
        echo "stderr tail output: \${stderr_log}"
        echo "------------------"
        tail "\${stderr_log}"
    fi
    echo ""
)

module_failed()
(
    local module="\$1"
    local exit_log="\${log_dir}/\${module}/exit_code"
    local stderr_log="\${log_dir}/\${module}/stderr.log"

    if [ -f "\${exit_log}" ] && [ "\$(cat "\${exit_log}")" != "0" ]; then
        return 0
    fi
    if [ -f "\${stderr_log}" ] && grep -qE "^==[0-9]+==ERROR: (AddressSanitizer|LeakSanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer):|^SUMMARY: AddressSanitizer:" "\${stderr_log}" 2>/dev/null; then
        return 0
    fi
    return 1
)

print_failures()
(
    local any_failure=0

    for module in \$module_list;
    do
        if module_failed "\${module}"; then
            any_failure=1
            break
        fi
    done

    if [ "\${any_failure}" -eq 1 ]; then
        echo ""
        echo "==================== TEST FAILURES ===================="
        for module in \$module_list;
        do
            if module_failed "\${module}"; then
                print_module_failure "\${module}"
            fi
        done
        echo "======================================================="
        return 1
    fi
)

execute_tests()
(
    for module in \$module_list;
    do
        module_path=$PROJECT_ROOT_PATH/\$module
        test_bin_path=$TEST_BIN_PATH/${APP_NAME}_\$module

        repeat_args=
        if [ ! -z "\$REPEAT" ]; then
            repeat_args="--gtest_repeat="\$REPEAT" --gtest_throw_on_failure"
        fi

        cmd="env \$DEBUGGER \$DEBUGGER_ARGS \$test_bin_path \\
                --gtest_death_test_style=threadsafe \\
                --gtest_shuffle \\
                --gtest_color=yes \\
                --gtest_filter="*\$test_filter*" \\
                \$repeat_args \$TEST_ARGS"

        stdout_log="\${log_dir}/\${module}/stdout.log"
        stderr_log="\${log_dir}/\${module}/stderr.log"
        exit_log="\${log_dir}/\${module}/exit_code"

        echo ""
        echo "###################################################################################"
        echo "# testing module: \$module"
        echo "# test command: \$cmd"
        echo "###################################################################################"
        echo ""

        mkdir -p "\${log_dir}/\${module}"
        (cd \$module_path && \$cmd; echo \$? > "\${exit_log}") \\
            1> >(tee "\${stdout_log}") \\
            2> >(tee "\${stderr_log}" >&2)
        wait
    done

    print_failures
)

list_tests()
(
    modules=( \$module_list )
    modules_count=\${#modules[@]}

    for module in \$module_list;
    do
        if [ \$modules_count -gt 1 ]; then
            echo ""
            echo "Module \$module tests:"
            echo ""
        fi

        test_bin_path=$TEST_BIN_PATH/${APP_NAME}_\$module

        \$test_bin_path --gtest_list_tests
    done
)

case \$1 in
    test)
        execute_tests
        ;;
    list)
        list_tests
        ;;
    *)
        false
        ;;
esac

EOF
)

echo "generating test executor in: $TEST_PATH/$executor_name"
generate_test
chmod +x $TEST_PATH/$executor_name