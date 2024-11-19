function fetch_lua()
{
    LUA_VERSION=5.4.7
    LUA_NAME=lua-$LUA_VERSION
    LUA_DIR=depend/$LUA_NAME
    if ! [ -d "$LUA_DIR" ]; then
        echo "## fetch_lua $LUA_NAME"
        curl -fsSL http://www.lua.org/ftp/$LUA_NAME.tar.gz -o depend/$LUA_NAME.tar.gz 
        tar zxf depend/$LUA_NAME.tar.gz -C depend
        cp -r script/patch/lua.cmake $LUA_DIR/CMakeLists.txt
    fi
}

function fetch_wxwidgets()
{
    WXWIDGETS_VERSION=3.2.6
    WXWIDGETS_NAME=wxWidgets-${WXWIDGETS_VERSION}
    WXWIDGETS_DIR=depend/$WXWIDGETS_NAME
    if ! [ -d "$WXWIDGETS_DIR" ]; then
        echo "## fetch_wxwidgets $WXWIDGETS_NAME"
        curl -fsSL https://github.com/wxWidgets/wxWidgets/releases/download/v$WXWIDGETS_VERSION/$WXWIDGETS_NAME.7z -o depend/$WXWIDGETS_NAME.7z
        mkdir -p $WXWIDGETS_DIR
        7z x depend/$WXWIDGETS_NAME.7z -o$WXWIDGETS_DIR
    fi
}

function fetch_stb()
{
    STB_VERSION=lastest
    STB_NAME=stb-${STB_VERSION}
    STB_DIR=depend/$STB_NAME
    if ! [ -d "$STB_DIR" ]; then
        echo "## fetch_stb $STB_NAME"
        curl -fsSL https://github.com/nothings/stb/archive/refs/heads/master.zip -o depend/$STB_NAME.7z
        7z x depend/$STB_NAME.7z
        mv -f stb-master $STB_DIR
    fi
}