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
    elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin") # macOS
        # Find wxWidgets
        find_package(wxWidgets REQUIRED COMPONENTS core base)
        # Include wxWidgets' settings (e.g., include directories, flags)
        include(${wxWidgets_USE_FILE})
        # Find wxWidgets
        find_package(wxWidgets REQUIRED COMPONENTS core base propgrid)

        if(wxWidgets_FOUND)
            include(${wxWidgets_USE_FILE})
            target_link_libraries(${PROJECT_NAME} PRIVATE ${wxWidgets_LIBRARIES})
        else()
            message(FATAL_ERROR "wxWidgets not found!")
        endif()
        target_link_libraries(${PROJECT_NAME} PRIVATE
            lua
            cjson
        )
        target_compile_definitions(${TARGET_NAME} PRIVATE
            -D__APPLE__
        )
        target_link_libraries(${TARGET_NAME} PRIVATE
            "-framework Cocoa" # For GUI
            "-framework IOKit" # Optional: For hardware interaction
            "-framework CoreFoundation" # Core macOS utilities
            "-framework Carbon" # Legacy support if required
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
    if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14") # Minimum macOS version
    endif()
endif()

# add lua
set(LUA_CODE_DIR depend/lua-5.4.7)
add_subdirectory(${LUA_CODE_DIR})

# add cjson
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(ENABLE_CJSON_TEST OFF)
set(ENABLE_CUSTOM_COMPILER_FLAGS OFF)
set(BUILD_SHARED_LIBS OFF)
set(CJSON_CODE_DIR depend/cJSON-1.7.18)
add_subdirectory(${CJSON_CODE_DIR})

# add wxwidgets
set(WXWIDGETS_CODE_DIR depend/wxWidgets-3.2.6)
set(wxBUILD_SHARED OFF)
set(INGORED_WARNINGS  "-Wno-unused-command-line-argument -Wno-deprecated-declarations -Wno-inconsistent-missing-override -Wno-cast-calling-convention -Wno-format"
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${INGORED_WARNINGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${INGORED_WARNINGS}")
if(NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
    add_subdirectory(${WXWIDGETS_CODE_DIR})
endif()

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
    ${CJSON_CODE_DIR}
)
if(NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # find_package(OpenMP REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        lua
        cjson
        wx::core
        wx::base
        wx::propgrid
        # OpenMP::OpenMP_CXX
    )
endif()

config_platform(${PROJECT_NAME})