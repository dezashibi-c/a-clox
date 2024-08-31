###############################################################################
# GENERAL CONFIGURATIONS AND UTILITIES
###############################################################################
cmake_minimum_required(VERSION 3.20)
set(CMAKE_C_STANDARD 11)
include(cmake/utils.cmake)

###############################################################################
# PROJECT SETUP
###############################################################################
project(CLoxCompiler LANGUAGES C)
include_directories(include src extern)
include(cmake/configs.cmake)
add_subdirectory(tests)

###############################################################################
# EXECUTABLE TARGETS
###############################################################################
add_executable(clox
    src/main.c
    src/chunk.c
    src/memory.c
    src/debug.c
    src/value.c
    src/vm.c
    src/compiler.c
    src/scanner.c
    src/object.c
    src/table.c
)

define_macro_option(clox NAN_BOXING ON)
define_macro_option(clox DEBUG_PRINT_CODE OFF)
define_macro_option(clox DEBUG_TRACE_EXECUTION OFF)
define_macro_option(clox DEBUG_STRESS_GC ON)
define_macro_option(clox DEBUG_LOG_GC OFF)

###############################################################################
# PRE BUILD TESTS
###############################################################################

# enable_pre_build_testing(main_exe_file all_tests)
