cmake_minimum_required(VERSION 3.16)
project(neon_arena C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG QUIET)

add_executable(neon_arena
    src/main.cpp
    src/glrender.cpp
    src/world.cpp
    src/game.cpp
    src/ui.cpp
    src/audio.cpp
)

if(glfw3_FOUND)
    target_link_libraries(neon_arena PRIVATE glfw)
else()
    find_library(GLFW_LIB glfw3 REQUIRED)
    target_link_libraries(neon_arena PRIVATE ${GLFW_LIB})
endif()

target_link_libraries(neon_arena PRIVATE OpenGL::GL)
target_include_directories(neon_arena PRIVATE src)

# OpenAL is optional: if headers are missing, the game runs without sound.
include(CheckIncludeFile)
check_include_file(AL/al.h HAVE_AL_H)
if(HAVE_AL_H)
    find_library(OPENAL_LIB openal)
    if(OPENAL_LIB)
        target_compile_definitions(neon_arena PRIVATE HAVE_OPENAL=1)
        target_link_libraries(neon_arena PRIVATE ${OPENAL_LIB})
    endif()
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(neon_arena PRIVATE dl)
endif()
