cmake_minimum_required(VERSION 3.25)
project(atmosim LANGUAGES CXX)

option(BUILD_GUI OFF)
option(BUILD_TUI ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Test")
    set(BUILD_TUI FALSE)
    set(BUILD_GUI FALSE)
endif()

if(DEFINED EMSCRIPTEN)
    set(IS_WEB_BUILD TRUE)
    set(BUILD_GUI TRUE)
    set(BUILD_TUI FALSE)
else()
    set(IS_WEB_BUILD FALSE)
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Test;Release;Web" CACHE STRING "Configuration types" FORCE)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/libs)

file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")
list(FILTER LIB_SOURCES EXCLUDE REGEX "main_.*\\.cpp$")

if(BUILD_TUI)
    add_executable(atmosim src/main_tui.cpp ${LIB_SOURCES})
endif()

# Pull GUI deps if GUI
if(BUILD_GUI)
    add_executable(atmosim_gui src/main_gui.cpp)

    # Emscripten provides its own
    if(NOT IS_WEB_BUILD)
        find_package(PkgConfig REQUIRED)
        find_package(OpenGL REQUIRED)
        pkg_check_modules(GLFW REQUIRED glfw3)
        pkg_check_modules(IMGUI REQUIRED imgui)

        target_link_libraries(atmosim_gui PRIVATE atmosim_lib ${IMGUI_LDFLAGS} ${GLFW_LDFLAGS} OpenGL::GL)
        target_include_directories(atmosim_gui PRIVATE ${IMGUI_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})
    else()
        include(FetchContent)
        # Pull ImGUI
        FetchContent_Declare(
            ImGUI
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG v1.92.6
            GIT_SHALLOW 1
        )
        FetchContent_MakeAvailable(ImGUI)

        target_sources(atmosim_gui PRIVATE
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        )

        target_compile_definitions(atmosim_gui PRIVATE IMGUI_IMPL_OPENGL_ES3)

        target_link_libraries(atmosim_gui PRIVATE atmosim_lib)

        target_compile_options(atmosim_gui PRIVATE -sUSE_GLFW=3 -sUSE_WEBGL2=1)
        target_link_options(atmosim_gui PRIVATE -sUSE_GLFW=3 -sUSE_WEBGL2=1)
        target_include_directories(atmosim_gui PRIVATE ${imgui_SOURCE_DIR})
    endif()

endif()

set(MAIN_TARGETS "")
if(BUILD_TUI)
    list(APPEND MAIN_TARGETS atmosim)
endif()
if(BUILD_GUI)
    list(APPEND MAIN_TARGETS atmosim_gui)
endif()

foreach(TARGET_NAME IN LISTS MAIN_TARGETS)
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_link_options(${TARGET_NAME} PRIVATE -static -static-libgcc -static-libstdc++)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -pedantic -g)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(${TARGET_NAME} PRIVATE -O3 -ffast-math -flto=auto)
        target_link_options(${TARGET_NAME} PRIVATE -flto=auto -s)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Web")
        # Web build flags
        target_compile_options(${TARGET_NAME} PRIVATE -O3 -ffast-math)
        target_link_options(${TARGET_NAME} PRIVATE
            # no LTO on web build
            -sEXPORTED_RUNTIME_METHODS=['callMain','ccall','cwrap','stringToUTF8','UTF8ToString','FS']
            -sMODULARIZE=1
            -sFULL_ES3=1
            -sENVIRONMENT=web,worker
            -sNO_EXIT_RUNTIME=1
            -sEXPORT_NAME='createAtmosim'
            -sNO_DISABLE_EXCEPTION_CATCHING
        )
    endif()
endforeach()

if(CMAKE_BUILD_TYPE STREQUAL "Test")
    # Apply optimization flags for the main targets in Test mode
    foreach(TARGET_NAME IN LISTS MAIN_TARGETS)
        target_compile_options(${TARGET_NAME} PRIVATE -O3 -ffast-math -flto=auto)
        target_link_options(${TARGET_NAME} PRIVATE -flto=auto)
    endforeach()

    file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
    find_package(Catch2 REQUIRED)
    add_executable(tests ${TEST_SOURCES})
    target_link_libraries(tests PRIVATE atmosim_lib Catch2::Catch2WithMain)
    target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests include libs)
    enable_testing()
    add_test(NAME AllTests COMMAND tests)
endif()
