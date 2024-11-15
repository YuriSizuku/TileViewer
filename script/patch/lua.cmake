set(LUA_CODE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(LUA_CODE     
    lapi.c lcode.c lctype.c ldebug.c ldo.c
    ldump.c lfunc.c lgc.c llex.c lmem.c lutf8lib.c linit.c
    lobject.c lopcodes.c lparser.c lstate.c lstring.c
    ltable.c ltm.c lundump.c lvm.c lzio.c
    lauxlib.c lbaselib.c lcorolib.c ldblib.c liolib.c
    lmathlib.c loadlib.c loslib.c lstrlib.c ltablib.c
)
list(TRANSFORM LUA_CODE PREPEND ${LUA_CODE_DIR}/)
add_library(lua STATIC ${LUA_CODE})