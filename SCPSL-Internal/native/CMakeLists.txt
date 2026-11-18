cmake_minimum_required(VERSION 3.20)
project(SLInternalNative LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "x64 only: configure with -A x64")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Everything lands in ../dist next to the managed SLInternal.dll.
set(SLI_DIST "${CMAKE_CURRENT_SOURCE_DIR}/../dist")

add_compile_definitions(UNICODE _UNICODE NOMINMAX)
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
endif()

add_library(SLBootstrap SHARED bootstrap/dllmain.cpp bootstrap/mono_api.h)
add_executable(SLInjector injector/main.cpp)

foreach(t SLBootstrap SLInjector)
    set_target_properties(${t} PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "$<1:${SLI_DIST}>"
        LIBRARY_OUTPUT_DIRECTORY "$<1:${SLI_DIST}>")
endforeach()

if(MINGW)
    target_link_options(SLBootstrap PRIVATE -static)
    target_link_options(SLInjector PRIVATE -static -municode)
endif()
