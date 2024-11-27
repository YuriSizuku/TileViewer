# default for mingw64
if [ -z "$CC" ]; then CC=gcc; fi
if [ -z "$CXX" ]; then CXX=g++; fi
if [ -z "$BUILD_DIR" ]; then BUILD_DIR=$(pwd)/build_linux64; fi
if [ -z "$BUILD_TYPE" ]; then BUILD_TYPE=Debug; fi

CORE_NUM=$(cat /proc/cpuinfo | grep -c ^processor)
echo "## CC=$CC BUILD_DIR=$BUILD_DIR BUILD_TYPE=$BUILD_TYPE"

# fetch depends
mkdir -p depend
source script/fetch_depend.sh
fetch_lua
fetch_cjson
fetch_wxwidgets

# build project
cmake -G "Unix Makefiles" -B $BUILD_DIR \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX
    
make -C $BUILD_DIR -j$CORE_NUM