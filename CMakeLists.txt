cmake_minimum_required(VERSION 3.25)

project(razmut_recreate VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "build type" FORCE)
endif()

add_library(rzm_core STATIC
  src/common.cpp
  src/pe.cpp
  src/scan.cpp
  src/patch.cpp
  src/state.cpp
  src/cli.cpp
  src/tui.cpp)

target_include_directories(rzm_core PUBLIC src)

if(MSVC)
  target_compile_options(rzm_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(rzm_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()

if(WIN32)
  target_compile_definitions(rzm_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

add_executable(razmut-recreate src/main.cpp)
target_link_libraries(razmut-recreate PRIVATE rzm_core)

if(MINGW)
  target_link_options(razmut-recreate PRIVATE -static -static-libgcc -static-libstdc++)
endif()

option(RZM_TESTS "build the test binary" ON)
if(RZM_TESTS)
  enable_testing()
  add_executable(rzm_tests tests/main.cpp)
  target_link_libraries(rzm_tests PRIVATE rzm_core)
  if(MINGW)
    target_link_options(rzm_tests PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  add_test(NAME rzm_tests COMMAND rzm_tests)
endif()
