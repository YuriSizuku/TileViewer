# default for mingw64
if [ -z "$CC" ]; then CC=clang; fi
if [ -z "$CXX" ]; then CXX=clang++; fi
if [ -z "$BUILD_DIR" ]; then BUILD_DIR=$(pwd)/build_darwin; fi
if [ -z "$BUILD_TYPE" ]; then BUILD_TYPE=Debug; fi

# CORE_NUM=$(cat /proc/cpuinfo | grep -c ^processor)
echo "## CC=$CC BUILD_DIR=$BUILD_DIR BUILD_TYPE=$BUILD_TYPE"

# fetch depends
mkdir -p depend
source script/fetch_depend.sh
fetch_lua
fetch_wxwidgets

# build project
cmake -G "Unix Makefiles" -B $BUILD_DIR \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX
    
make -C $BUILD_DIR -j8