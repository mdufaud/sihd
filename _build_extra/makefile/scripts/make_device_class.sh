#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

SRC_TEMPLATE=$SCRIPT_DIR/../templates/device/src_template.txt \
    HDR_TEMPLATE=$SCRIPT_DIR/../templates/device/hdr_template.txt \
    bash $MAKEFILE_TOOLS_SCRIPTS/make_class.sh $1 $2 $3