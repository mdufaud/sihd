#!/bin/bash

if [ -z "$1" ]; then
    echo "Project name is not set"
    exit 1;
fi
if [ -z "$2" ]; then
    echo "Module name is not set"
    exit 1;
fi
if [ -z "$3" ]; then
    echo "Test name is not set"
    exit 1;
fi

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

PROJ_NAME=$1
MODULE_NAME=$2
TEST_NAME=$3
TEST_FILENAME=$MODULE_NAME/test/Test$3.cpp

echo "Making new test $TEST_NAME for module $MODULE_NAME"
cp $SCRIPT_DIR/test_template.txt $TEST_FILENAME
sed -i "s/PROJ_NAME/$PROJ_NAME/g" $TEST_FILENAME
sed -i "s/MODULE_NAME/$MODULE_NAME/g" $TEST_FILENAME
sed -i "s/CLASS_NAME/$TEST_NAME/g" $TEST_FILENAME
echo "Done"