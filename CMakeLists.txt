cmake_minimum_required(VERSION 3.12)
project(TileViewer)

function(config_platform TARGET_NAME)
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        target_link_libraries(${TARGET_NAME} PRIVATE
            X11 # must below wxWidgets
            -static-libstdc++
            -static-libgcc
        )
    elseif(CMAKE_SYSTEM_NAME MATCHES "Windows") # currently only support mingw on windows
        target_sources(${TARGET_NAME} PRIVATE 
            asset/icon/icon.rc
        )
        target_compile_definitions(${TARGET_NAME} PRIVATE 
            -D_WIN32_WINNT=0X0400
        )
        target_link_libraries(${TARGET_NAME} PRIVATE
            -Wl,--subsystem,windows:4.0
            -static-libstdc++
            -static-libgcc
        )
    else()
        message("${CMAKE_SYSTEM_NAME} enviroment for ${TARGET_NAME} not supported!")
    endif()
endfunction()

# global config
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    if(CMAKE_C_COMPILER MATCHES i686 OR CMAKE_C_COMPILER MATCHES i386) # multi arch for x86
        set(CMAKE_SYSTEM_LIBRARY_PATH /usr/lib/i386-linux-gnu) # this is very important
        set(FIND_LIBRARY_USE_LIB64_PATHS OFF)
        set(CMAKE_IGNORE_PATH /lib /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)
        set(wxUSE_LIBJPEG OFF)
    endif()
endif()

# add lua
set(LUA_CODE_DIR depend/lua-5.4.7)
add_subdirectory(${LUA_CODE_DIR})

# add wxwidgets
set(WXWIDGETS_CODE_DIR depend/wxWidgets-3.2.6)
set(wxBUILD_SHARED OFF)
set(INGORED_WARNINGS  "-Wno-unused-command-line-argument -Wno-deprecated-declarations -Wno-inconsistent-missing-override -Wno-cast-calling-convention -Wno-format"
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${INGORED_WARNINGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${INGORED_WARNINGS}")
add_subdirectory(${WXWIDGETS_CODE_DIR})

# define wxwidgets xrc
set(wxrc $<TARGET_FILE:wxrc>)
set(TILEVIEWER_UI_XRC 
    ${CMAKE_SOURCE_DIR}/asset/ui/ui_main.xrc
)
set(TILEVIEWER_UI_CODE 
    ${CMAKE_CURRENT_BINARY_DIR}/ui_resource.cpp
)
add_custom_command(
    OUTPUT ${TILEVIEWER_UI_CODE}
    COMMAND ${wxrc} -c -o ${TILEVIEWER_UI_CODE} ${TILEVIEWER_UI_XRC}
    DEPENDS ${TILEVIEWER_UI_XRC}
    DEPENDS wxrc                        # Not needed with the installed version.
    COMMENT "Compiling XRC resources"
)

# define tileviewer
set(CMAKE_CXX_STANDARD 11)
set(TILEVIEWER_CODE
    src/core_app.cpp
    src/core_solver.cpp
    src/plugin_builtin.c
    src/plugin_lua.c
    src/ui_top.cpp
    src/ui_menu.cpp
    src/ui_config.cpp
    src/ui_tile.cpp
)
add_executable(${PROJECT_NAME}
    ${TILEVIEWER_CODE}
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${LUA_CODE_DIR}/src
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    lua
    wx::core 
    wx::base
    wx::propgrid
)
config_platform(${PROJECT_NAME})