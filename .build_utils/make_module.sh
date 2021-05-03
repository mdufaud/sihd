#!/bin/bash

if [ -z "$1" ]; then
    echo "Project name is not set"
    exit 1;
fi
if [ -z "$2" ]; then
    echo "Module name is not set"
    exit 1;
fi

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

PROJ_NAME=$1
MODULE_NAME=$2
echo "Building module $MODULE_NAME"
mkdir -p $MODULE_NAME/src $MODULE_NAME/include/$PROJ_NAME/$MODULE_NAME $MODULE_NAME/test
cp $SCRIPT_DIR/scons_template.txt $MODULE_NAME/scons.py
cp $SCRIPT_DIR/test_main_template.txt $MODULE_NAME/test/main.cpp
echo "Module built"