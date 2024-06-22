#!/bin/bash

executor_name=execute_tests.sh

generate_test()
(
    cd $TEST_PATH
    cat << EOF > $executor_name
cd $TEST_PATH
source .env

module_list="\$2"
if [ -z "\$module_list" ]; then
    module_list=\$(cd $TEST_BIN_PATH && ls)
    module_list="\${module_list//${APP_NAME}_/}"
fi
test_filter="\$3"

execute_tests()
(
    for module in \$module_list;
    do
        module_path=$HERE/\$module
        test_bin_path=$TEST_BIN_PATH/${APP_NAME}_\$module

        repeat_args=
        if [ ! -z "\$REPEAT" ]; then
            repeat_args="--gtest_repeat="\$REPEAT" --gtest_throw_on_failure"
        fi

        cmd="env \$DEBUGGER \$DEBUGGER_ARGS \$test_bin_path --gtest_death_test_style=threadsafe --gtest_shuffle --gtest_filter="*\$test_filter*" \$repeat_args \$TEST_ARGS"

        echo ""
        echo "###################################################################################"
        echo "# testing module: \$module"
        echo "# test command: \$cmd"
        echo "###################################################################################"
        echo ""

        (cd \$module_path && \$cmd)
    done
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
        return 1
        ;;
esac

EOF
)

echo "generating test executor in: $TEST_PATH/$executor_name"
generate_test