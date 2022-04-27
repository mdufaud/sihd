cd $TEST_PATH
echo "generating environnement variables in: `pwd`/.env"
echo "export APP_NAME="$APP_NAME > .env
echo "export TEST_PATH="$TEST_PATH >> .env
echo "export BUILD_PATH="$BUILD_PATH >> .env
echo "export LIB_PATH="$LIB_PATH >> .env
echo "export BIN_PATH="$BIN_PATH >> .env
echo "export ETC_PATH="$ETC_PATH >> .env
echo "export SHARE_PATH="$SHARE_PATH >> .env
echo "export EXTLIB_PATH="$EXTLIB_PATH >> .env
echo "export EXTLIB_LIB_PATH="$EXTLIB_LIB_PATH >> .env
cd - > /dev/null