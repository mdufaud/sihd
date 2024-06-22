#!/bin/bash

generate_env()
(
    cd $TEST_PATH
    cat << EOF > .env
export APP_NAME="$APP_NAME"
export TEST_PATH="$TEST_PATH"
export BUILD_PATH="$BUILD_PATH"
export LIB_PATH="$LIB_PATH"
export BIN_PATH="$BIN_PATH"
export ETC_PATH="$ETC_PATH"
export SHARE_PATH="$SHARE_PATH"
export EXTLIB_PATH="$EXTLIB_PATH"
export EXTLIB_LIB_PATH="$EXTLIB_LIB_PATH"
EOF
)

echo "generating environnement variables in: $TEST_PATH/.env"
generate_env