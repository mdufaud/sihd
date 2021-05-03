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
    echo "Class name is not set"
    exit 1;
fi

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

PROJ_NAME=$1
MODULE_NAME=$2
CLASS_NAME=$3
CLASS_SRC_FILENAME=$MODULE_NAME/src/$3.cpp
CLASS_HDR_FILENAME=$MODULE_NAME/include/$PROJ_NAME/$MODULE_NAME/$3.hpp

echo "Making new class $CLASS_NAME for module $MODULE_NAME"
cp $SCRIPT_DIR/class_src_template.txt $CLASS_SRC_FILENAME
cp $SCRIPT_DIR/class_hdr_template.txt $CLASS_HDR_FILENAME
sed -i "s/PROJ_NAME/$PROJ_NAME/g" $CLASS_SRC_FILENAME
sed -i "s/MODULE_NAME/$MODULE_NAME/g" $CLASS_SRC_FILENAME
sed -i "s/CLASS_NAME/$CLASS_NAME/g" $CLASS_SRC_FILENAME

sed -i "s/PROJ_NAME_UPPER/${PROJ_NAME^^}/g" $CLASS_HDR_FILENAME
sed -i "s/MODULE_NAME_UPPER/${MODULE_NAME^^}/g" $CLASS_HDR_FILENAME
sed -i "s/CLASS_NAME_UPPER/${CLASS_NAME^^}/g" $CLASS_HDR_FILENAME
sed -i "s/PROJ_NAME/$PROJ_NAME/g" $CLASS_HDR_FILENAME
sed -i "s/MODULE_NAME/$MODULE_NAME/g" $CLASS_HDR_FILENAME
sed -i "s/CLASS_NAME/$CLASS_NAME/g" $CLASS_HDR_FILENAME
echo "Done"