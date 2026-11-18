cmake_minimum_required(VERSION 3.20)
project(SLMenu LANGUAGES C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "SLMenu targets x64 only (the game is 64-bit). Configure with -A x64.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  # Static CRT: the DLL must not depend on a VC++ redistributable inside the game process.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(minhook STATIC
  third_party/minhook/src/buffer.c
  third_party/minhook/src/hook.c
  third_party/minhook/src/trampoline.c
  third_party/minhook/src/hde/hde32.c
  third_party/minhook/src/hde/hde64.c)
target_include_directories(minhook PUBLIC third_party/minhook/include)

add_library(SLMenu SHARED
  src/dllmain.cpp
  src/esp.cpp
  src/game.cpp
  src/gui.cpp
  src/il2cpp_api.cpp
  src/log.cpp
  src/menu.cpp
  src/runtime.cpp)
target_link_libraries(SLMenu PRIVATE minhook psapi)
target_compile_definitions(SLMenu PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
set_target_properties(SLMenu PROPERTIES PREFIX "")

add_executable(SLInject injector/injector.cpp)
target_compile_definitions(SLInject PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

if(MSVC)
  target_compile_options(SLMenu PRIVATE /W4 /permissive- /utf-8 /EHsc)
  target_compile_options(SLInject PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(SLMenu PRIVATE -Wall -Wextra -Wno-cast-function-type)
  target_link_options(SLMenu PRIVATE -static -static-libgcc -static-libstdc++)
  target_compile_options(SLInject PRIVATE -Wall -Wextra -municode)
  target_link_options(SLInject PRIVATE -static -municode)
endif()
