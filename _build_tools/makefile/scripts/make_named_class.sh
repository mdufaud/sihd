#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

SRC_TEMPLATE=$SCRIPT_DIR/../templates/named/src_template.txt \
    HDR_TEMPLATE=$SCRIPT_DIR/../templates/named/hdr_template.txt \
    bash $SCRIPT_DIR/make_class.sh $1 $2 $3