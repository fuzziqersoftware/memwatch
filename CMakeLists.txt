cmake_minimum_required(VERSION 3.10)



# Project setup

project(memwatch)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

include_directories("/usr/local/include")
link_directories("/usr/local/lib")



find_path(READLINE_INCLUDE_DIR readline/readline.h)
find_library(READLINE_LIBRARY NAMES readline)
add_library(readline SHARED IMPORTED GLOBAL)
set_target_properties(readline PROPERTIES IMPORTED_LOCATION "${READLINE_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${READLINE_INCLUDE_DIR}")



# Executable definitions

add_executable(memwatch
  src/Main.cc
  src/MemorySearch.cc
  src/MemwatchShell.cc
  src/ProcessMemoryAdapter.cc
  src/RegionFreezer.cc
  src/Signalable.cc
)
target_link_libraries(memwatch phosg readline)

add_executable(value_test src/tests/value_test.cc)
add_executable(value_test_float src/tests/value_test_float.cc)
add_executable(array_test src/tests/array_test.cc)



# Installation configuration

install(TARGETS memwatch DESTINATION bin)
